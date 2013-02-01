# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys, os, re, simplejson

class DocumentationItemInfo(object):
    def __init__(self, env_root, md_path, filename):
        self.env_root = env_root
        # full path to MD file, without filename
        self.source_path = md_path
        # MD filename
        self.source_filename = filename

    def env_root(self):
        return self.env_root

    def source_path(self):
        return self.source_path

    def source_filename(self):
        return self.source_filename

    def base_filename(self):
        return self.source_filename[:-len(".md")]

    def source_path_and_filename(self):
        return os.sep.join([self.source_path, self.source_filename])

    def source_path_relative_from_env_root(self):
        return self.source_path[len(self.env_root) + 1:]

class DevGuideItemInfo(DocumentationItemInfo):
    def __init__(self, env_root, devguide_root, md_path, filename):
        DocumentationItemInfo.__init__(self, env_root, md_path, filename)
        self.devguide_root = devguide_root

    def source_path_relative_from_devguide_root(self):
        return self.source_path[len(self.devguide_root) + 1:]

    def destination_path(self):
        root_pieces = self.devguide_root.split(os.sep)
        root_pieces[-1] = "dev-guide"
        return os.sep.join([os.sep.join(root_pieces), self.source_path_relative_from_devguide_root()])

class ModuleInfo(DocumentationItemInfo):
    def __init__(self, env_root, module_root, md_path, filename):
        DocumentationItemInfo.__init__(self, env_root, md_path, filename)
        self.module_root = module_root
        self.metadata = self.get_metadata()

    def remove_comments(self, text):
        def replacer(match):
            s = match.group(0)
            if s.startswith('/'):
                return ""
            else:
                return s
        pattern = re.compile(
            r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
            re.DOTALL | re.MULTILINE
        )
        return re.sub(pattern, replacer, text)

    def get_metadata(self):
        if self.level() == "third-party":
            return simplejson.loads("{}")
        try:
            js = unicode(open(self.js_module_path(),"r").read(), 'utf8')
        except IOError:
            raise Exception, "JS module: '" + path_to_js + \
                             "', corresponding to documentation file: '"\
                             + self.source_path_and_filename() + "' wasn't found"
        js = self.remove_comments(js)
        js_lines = js.splitlines(True)
        metadata = ''
        reading_metadata = False
        for line in js_lines:
            if reading_metadata:
                if line.startswith("};"):
                    break
                metadata += line
                continue
            if line.startswith("module.metadata"):
                reading_metadata = True
        metadata = metadata.replace("'", '"')
        return simplejson.loads("{" + metadata + "}")

    def source_path_relative_from_module_root(self):
        return self.source_path[len(self.module_root) + 1:]

    def destination_path(self):
        if self.level() == "third-party":
            return os.sep.join([self.env_root, "doc", "modules", "packages"])
        root_pieces = self.module_root.split(os.sep)
        root_pieces[-1] = "modules"
        relative_pieces = self.source_path_relative_from_module_root().split(os.sep)
        return os.sep.join(root_pieces + relative_pieces)

    def js_module_path(self):
        return os.path.join(self.env_root, "lib", \
                            self.source_path_relative_from_module_root(), \
                            self.source_filename[:-len(".md")] + ".js")

    def relative_url(self):
        if self.level() == "third-party":
            relative_pieces = ["packages"]
        else:
            relative_pieces = self.source_path_relative_from_module_root().split(os.sep)
        return "/".join(relative_pieces) + "/" + self.base_filename() + ".html"

    def name(self):
        if os.sep.join([self.module_root, "sdk"]) == self.source_path or self.level() == "third-party":
            return self.source_filename[:-3]
        else:
            path_from_root_pieces = self.source_path_relative_from_module_root().split(os.sep)
            return "/".join(["/".join(path_from_root_pieces[1:]), self.source_filename[:-len(".md")]])

    def level(self):
        if self.source_path_relative_from_env_root().startswith("packages"):
            return "third-party"
        else:
            if os.sep.join([self.module_root, "sdk"]) == self.source_path:
                return "high"
            else:
                return "low"

def get_modules_in_package(env_root, package_docs_dir, module_list, ignore_files_in_root):
    for (dirpath, dirnames, filenames) in os.walk(package_docs_dir):
        for filename in filenames:
            # ignore files in the root
            if ignore_files_in_root and package_docs_dir == dirpath:
                continue
            if filename.endswith(".md"):
                module_list.append(ModuleInfo(env_root, package_docs_dir, dirpath, filename))

def get_module_list(env_root):
    module_list = []
    # get the built-in modules
    module_root = os.sep.join([env_root, "doc", "module-source"])
    get_modules_in_package(env_root, module_root, module_list, True)
    # get the third-party modules
    packages_root = os.sep.join([env_root, "packages"])
    if os.path.exists(packages_root):
        for entry in os.listdir(packages_root):
            if os.path.isdir(os.sep.join([packages_root, entry])):
                package_docs = os.sep.join([packages_root, entry, "docs"])
                if os.path.exists(package_docs):
                    get_modules_in_package(env_root, package_docs, module_list, False)
    module_list.sort(key=lambda x: x.name())
    return module_list

def get_devguide_list(env_root):
    devguide_list = []
    devguide_root = os.sep.join([env_root, "doc", "dev-guide-source"])
    for (dirpath, dirnames, filenames) in os.walk(devguide_root):
        for filename in filenames:
            if filename.endswith(".md"):
                devguide_list.append(DevGuideItemInfo(env_root, devguide_root, dirpath, filename))
    return devguide_list

if __name__ == "__main__":
    module_list = get_module_list(sys.argv[1])
    print [module_info.name for module_info in module_list]
