# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import shutil
import hashlib
import tarfile
import StringIO

from cuddlefish._version import get_versions
from cuddlefish.docs import apiparser
from cuddlefish.docs import apirenderer
from cuddlefish.docs import webdocs
from documentationitem import get_module_list
from documentationitem import get_devguide_list
from documentationitem import ModuleInfo
from documentationitem import DevGuideItemInfo
from linkrewriter import rewrite_links
import simplejson as json

DIGEST = "status.md5"
TGZ_FILENAME = "addon-sdk-docs.tgz"

def get_sdk_docs_path(env_root):
    return os.path.join(env_root, "doc")

def get_base_url(env_root):
    sdk_docs_path = get_sdk_docs_path(env_root).lstrip("/")
    return "file://"+"/"+"/".join(sdk_docs_path.split(os.sep))+"/"

def clean_generated_docs(docs_dir):
    status_file = os.path.join(docs_dir, "status.md5")
    if os.path.exists(status_file):
        os.remove(status_file)
    index_file = os.path.join(docs_dir, "index.html")
    if os.path.exists(index_file):
        os.remove(index_file)
    dev_guide_dir = os.path.join(docs_dir, "dev-guide")
    if os.path.exists(dev_guide_dir):
        shutil.rmtree(dev_guide_dir)
    api_doc_dir = os.path.join(docs_dir, "modules")
    if os.path.exists(api_doc_dir):
        shutil.rmtree(api_doc_dir)

def generate_static_docs(env_root, override_version=get_versions()["version"]):
    clean_generated_docs(get_sdk_docs_path(env_root))
    generate_docs(env_root, override_version, stdout=StringIO.StringIO())
    tgz = tarfile.open(TGZ_FILENAME, 'w:gz')
    tgz.add(get_sdk_docs_path(env_root), "doc")
    tgz.close()
    return TGZ_FILENAME

def generate_local_docs(env_root):
    return generate_docs(env_root, get_versions()["version"], get_base_url(env_root))

def generate_named_file(env_root, filename_and_path):
    module_list = get_module_list(env_root)
    web_docs = webdocs.WebDocs(env_root, module_list, get_versions()["version"], get_base_url(env_root))
    abs_path = os.path.abspath(filename_and_path)
    path, filename = os.path.split(abs_path)
    if abs_path.startswith(os.path.join(env_root, 'doc', 'module-source')):
        module_root = os.sep.join([env_root, "doc", "module-source"])
        module_info = ModuleInfo(env_root, module_root, path, filename)
        write_module_doc(env_root, web_docs, module_info, False)
    elif abs_path.startswith(os.path.join(get_sdk_docs_path(env_root), 'dev-guide-source')):
        devguide_root = os.sep.join([env_root, "doc", "dev-guide-source"])
        devguideitem_info = DevGuideItemInfo(env_root, devguide_root, path, filename)
        write_devguide_doc(env_root, web_docs, devguideitem_info, False)
    else:
        raise ValueError("Not a valid path to a documentation file")

def generate_docs(env_root, version=get_versions()["version"], base_url=None, stdout=sys.stdout):
    docs_dir = get_sdk_docs_path(env_root)
    # if the generated docs don't exist, generate everything
    if not os.path.exists(os.path.join(docs_dir, "dev-guide")):
        print >>stdout, "Generating documentation..."
        generate_docs_from_scratch(env_root, version, base_url)
        current_status = calculate_current_status(env_root)
        open(os.path.join(docs_dir, DIGEST), "w").write(current_status)
    else:
        current_status = calculate_current_status(env_root)
        previous_status_file = os.path.join(docs_dir, DIGEST)
        docs_are_up_to_date = False
        if os.path.exists(previous_status_file):
            docs_are_up_to_date = current_status == open(previous_status_file, "r").read()
        # if the docs are not up to date, generate everything
        if not docs_are_up_to_date:
            print >>stdout, "Regenerating documentation..."
            generate_docs_from_scratch(env_root, version, base_url)
            open(os.path.join(docs_dir, DIGEST), "w").write(current_status)
    return get_base_url(env_root) + "index.html"

# this function builds a hash of the name and last modification date of:
# * every file in "doc/sdk" which ends in ".md"
# * every file in "doc/dev-guide-source" which ends in ".md"
# * every file in "doc/static-files" which does not start with "."
def calculate_current_status(env_root):
    docs_dir = get_sdk_docs_path(env_root)
    current_status = hashlib.md5()
    module_src_dir = os.path.join(env_root, "doc", "module-source")
    for (dirpath, dirnames, filenames) in os.walk(module_src_dir):
        for filename in filenames:
            if filename.endswith(".md"):
                current_status.update(filename)
                current_status.update(str(os.path.getmtime(os.path.join(dirpath, filename))))
    guide_src_dir = os.path.join(docs_dir, "dev-guide-source")
    for (dirpath, dirnames, filenames) in os.walk(guide_src_dir):
        for filename in filenames:
            if filename.endswith(".md"):
                current_status.update(filename)
                current_status.update(str(os.path.getmtime(os.path.join(dirpath, filename))))
    package_dir = os.path.join(env_root, "packages")
    for (dirpath, dirnames, filenames) in os.walk(package_dir):
        for filename in filenames:
            if filename.endswith(".md"):
                current_status.update(filename)
                current_status.update(str(os.path.getmtime(os.path.join(dirpath, filename))))
    base_html_file = os.path.join(docs_dir, "static-files", "base.html")
    current_status.update(base_html_file)
    current_status.update(str(os.path.getmtime(os.path.join(dirpath, base_html_file))))
    return current_status.digest()

def generate_docs_from_scratch(env_root, version, base_url):
    docs_dir = get_sdk_docs_path(env_root)
    module_list = get_module_list(env_root)
    web_docs = webdocs.WebDocs(env_root, module_list, version, base_url)
    must_rewrite_links = True
    if base_url:
        must_rewrite_links = False
    clean_generated_docs(docs_dir)

    # py2.5 doesn't have ignore=, so we delete tempfiles afterwards. If we
    # required >=py2.6, we could use ignore=shutil.ignore_patterns("*~")
    for (dirpath, dirnames, filenames) in os.walk(docs_dir):
        for n in filenames:
            if n.endswith("~"):
                os.unlink(os.path.join(dirpath, n))

    # generate api docs for all modules
    if not os.path.exists(os.path.join(docs_dir, "modules")):
        os.mkdir(os.path.join(docs_dir, "modules"))
    [write_module_doc(env_root, web_docs, module_info, must_rewrite_links) for module_info in module_list]

    # generate third-party module index
    third_party_index_file = os.sep.join([env_root, "doc", "module-source", "third-party-modules.md"])
    third_party_module_list = [module_info for module_info in module_list if module_info.level() == "third-party"]
    write_module_index(env_root, web_docs, third_party_index_file, third_party_module_list, must_rewrite_links)


    # generate high-level module index
    high_level_index_file = os.sep.join([env_root, "doc", "module-source", "high-level-modules.md"])
    high_level_module_list = [module_info for module_info in module_list if module_info.level() == "high"]
    write_module_index(env_root, web_docs, high_level_index_file, high_level_module_list, must_rewrite_links)

    # generate low-level module index
    low_level_index_file = os.sep.join([env_root, "doc", "module-source", "low-level-modules.md"])
    low_level_module_list = [module_info for module_info in module_list if module_info.level() == "low"]
    write_module_index(env_root, web_docs, low_level_index_file, low_level_module_list, must_rewrite_links)

    # generate dev-guide docs
    devguide_list = get_devguide_list(env_root)
    [write_devguide_doc(env_root, web_docs, devguide_info, must_rewrite_links) for devguide_info in devguide_list]

    # make /md/dev-guide/welcome.html the top level index file
    doc_html = web_docs.create_guide_page(os.path.join(docs_dir, 'dev-guide-source', 'index.md'))
    write_file(env_root, doc_html, docs_dir, 'index', False)

def write_module_index(env_root, web_docs, source_file, module_list, must_rewrite_links):
    doc_html = web_docs.create_module_index(source_file, module_list)
    base_filename, extension = os.path.splitext(os.path.basename(source_file))
    destination_path = os.sep.join([env_root, "doc", "modules"])
    write_file(env_root, doc_html, destination_path, base_filename, must_rewrite_links)

def write_module_doc(env_root, web_docs, module_info, must_rewrite_links):
    doc_html = web_docs.create_module_page(module_info)
    write_file(env_root, doc_html, module_info.destination_path(), module_info.base_filename(), must_rewrite_links)

def write_devguide_doc(env_root, web_docs, devguide_info, must_rewrite_links):
    doc_html = web_docs.create_guide_page(devguide_info.source_path_and_filename())
    write_file(env_root, doc_html, devguide_info.destination_path(), devguide_info.base_filename(), must_rewrite_links)

def write_file(env_root, doc_html, dest_dir, filename, must_rewrite_links):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    dest_path_html = os.path.join(dest_dir, filename) + ".html"
    replace_file(env_root, dest_path_html, doc_html, must_rewrite_links)
    return dest_path_html

def replace_file(env_root, dest_path, file_contents, must_rewrite_links):
    if os.path.exists(dest_path):
        os.remove(dest_path)
    # before we copy the final version, we'll rewrite the links
    # I'll do this last, just because we know definitely what the dest_path is at this point
    if must_rewrite_links and dest_path.endswith(".html"):
        file_contents = rewrite_links(env_root, get_sdk_docs_path(env_root), file_contents, dest_path)
    open(dest_path, "w").write(file_contents)

