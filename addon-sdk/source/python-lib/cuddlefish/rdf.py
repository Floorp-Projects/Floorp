# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import xml.dom.minidom
import StringIO
import codecs
import glob

RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
EM_NS = "http://www.mozilla.org/2004/em-rdf#"

class RDF(object):
    def __str__(self):
        # real files have an .encoding attribute and use it when you
        # write() unicode into them: they read()/write() unicode and
        # put encoded bytes in the backend file. StringIO objects
        # read()/write() unicode and put unicode in the backing store,
        # so we must encode the output of getvalue() to get a
        # bytestring. (cStringIO objects are weirder: they effectively
        # have .encoding hardwired to "ascii" and put only bytes in
        # the backing store, so we can't use them here).
        #
        # The encoding= argument to dom.writexml() merely sets the
        # XML header's encoding= attribute. It still writes unencoded
        # unicode to the output file, so we have to encode it for
        # real afterwards.
        #
        # Also see: https://bugzilla.mozilla.org/show_bug.cgi?id=567660

        buf = StringIO.StringIO()
        self.dom.writexml(buf, encoding="utf-8")
        return buf.getvalue().encode('utf-8')

class RDFUpdate(RDF):
    def __init__(self):
        impl = xml.dom.minidom.getDOMImplementation()
        self.dom = impl.createDocument(RDF_NS, "RDF", None)
        self.dom.documentElement.setAttribute("xmlns", RDF_NS)
        self.dom.documentElement.setAttribute("xmlns:em", EM_NS)

    def _make_node(self, name, value, parent):
        elem = self.dom.createElement(name)
        elem.appendChild(self.dom.createTextNode(value))
        parent.appendChild(elem)
        return elem

    def add(self, manifest, update_link):
        desc = self.dom.createElement("Description")
        desc.setAttribute(
            "about",
            "urn:mozilla:extension:%s" % manifest.get("em:id")
            )
        self.dom.documentElement.appendChild(desc)

        updates = self.dom.createElement("em:updates")
        desc.appendChild(updates)

        seq = self.dom.createElement("Seq")
        updates.appendChild(seq)

        li = self.dom.createElement("li")
        seq.appendChild(li)

        li_desc = self.dom.createElement("Description")
        li.appendChild(li_desc)

        self._make_node("em:version", manifest.get("em:version"),
                        li_desc)

        apps = manifest.dom.documentElement.getElementsByTagName(
            "em:targetApplication"
            )

        for app in apps:
            target_app = self.dom.createElement("em:targetApplication")
            li_desc.appendChild(target_app)

            ta_desc = self.dom.createElement("Description")
            target_app.appendChild(ta_desc)

            for name in ["em:id", "em:minVersion", "em:maxVersion"]:
                elem = app.getElementsByTagName(name)[0]
                self._make_node(name, elem.firstChild.nodeValue, ta_desc)

            self._make_node("em:updateLink", update_link, ta_desc)

class RDFManifest(RDF):
    def __init__(self, path):
        self.dom = xml.dom.minidom.parse(path)

    def set(self, property, value):
        elements = self.dom.documentElement.getElementsByTagName(property)
        if not elements:
            raise ValueError("Element with value not found: %s" % property)
        if not elements[0].firstChild:
            elements[0].appendChild(self.dom.createTextNode(value))
        else:
            elements[0].firstChild.nodeValue = value

    def get(self, property, default=None):
        elements = self.dom.documentElement.getElementsByTagName(property)
        if not elements:
            return default
        return elements[0].firstChild.nodeValue

    def remove(self, property):
        elements = self.dom.documentElement.getElementsByTagName(property)
        if not elements:
            return True
        else:
            for i in elements:
                i.parentNode.removeChild(i);

        return True;

    def add_node(self, node):
        top =  self.dom.documentElement.getElementsByTagName("Description")[0];
        top.appendChild(node)


def gen_manifest(template_root_dir, target_cfg, jid, harness_options={},
                 update_url=None, bootstrap=True, enable_mobile=False):
    install_rdf = os.path.join(template_root_dir, "install.rdf")
    manifest = RDFManifest(install_rdf)
    dom = manifest.dom

    manifest.set("em:id", jid)
    manifest.set("em:version",
                 target_cfg.get('version', '1.0'))

    if "locale" in harness_options:
        # addon_title       -> <em:name>
        # addon_author      -> <em:creator>
        # addon_description -> <em:description>
        # addon_homepageURL -> <em:homepageURL>
        localizable_in = ["title", "author", "description", "homepage"]
        localized_out  = ["name", "creator", "description", "homepageURL"]
        for lang in harness_options["locale"]:
            desc = dom.createElement("Description")

            for value_in in localizable_in:
                key_in = "extensions." + target_cfg.get("id", "") + "." + value_in
                tag_out = localized_out[localizable_in.index(value_in)]

                if key_in in harness_options["locale"][lang]:
                    elem = dom.createElement("em:" + tag_out)
                    elem_value = harness_options["locale"][lang][key_in]
                    elem.appendChild(dom.createTextNode(elem_value))
                    desc.appendChild(elem)

            # Don't add language if no localizeable field was localized
            if desc.hasChildNodes():
                locale = dom.createElement("em:locale")
                locale.appendChild(dom.createTextNode(lang))
                desc.appendChild(locale)

                localized = dom.createElement("em:localized")
                localized.appendChild(desc)
                manifest.add_node(localized)

    manifest.set("em:name",
                 target_cfg.get('title', target_cfg.get('fullName', target_cfg['name'])))
    manifest.set("em:description",
                 target_cfg.get("description", ""))
    manifest.set("em:creator",
                 target_cfg.get("author", ""))

    if target_cfg.get("homepage"):
        manifest.set("em:homepageURL", target_cfg.get("homepage"))
    else:
        manifest.remove("em:homepageURL")

    manifest.set("em:bootstrap", str(bootstrap).lower())

    # XPIs remain packed by default, but package.json can override that. The
    # RDF format accepts "true" as True, anything else as False. We expect
    # booleans in the .json file, not strings.
    manifest.set("em:unpack", "true" if target_cfg.get("unpack") else "false")

    for translator in target_cfg.get("translators", [ ]):
        elem = dom.createElement("em:translator");
        elem.appendChild(dom.createTextNode(translator))
        manifest.add_node(elem)

    for developer in target_cfg.get("developers", [ ]):
        elem = dom.createElement("em:developer");
        elem.appendChild(dom.createTextNode(developer))
        dom.documentElement.getElementsByTagName("Description")[0].appendChild(elem)

    for contributor in target_cfg.get("contributors", [ ]):
        elem = dom.createElement("em:contributor");
        elem.appendChild(dom.createTextNode(contributor))
        manifest.add_node(elem)

    if update_url:
        manifest.set("em:updateURL", update_url)
    else:
        manifest.remove("em:updateURL")

    if target_cfg.get("preferences"):
        manifest.set("em:optionsType", "2")

        # workaround until bug 971249 is fixed
        # https://bugzilla.mozilla.org/show_bug.cgi?id=971249
        manifest.set("em:optionsURL", "data:text/xml,<placeholder/>")

        # workaround for workaround, for testing simple-prefs-regression
        if (os.path.exists(os.path.join(template_root_dir, "options.xul"))):
            manifest.remove("em:optionsURL")
    else:
        manifest.remove("em:optionsType")
        manifest.remove("em:optionsURL")

    if enable_mobile:
        target_app = dom.createElement("em:targetApplication")
        manifest.add_node(target_app)

        ta_desc = dom.createElement("Description")
        target_app.appendChild(ta_desc)

        elem = dom.createElement("em:id")
        elem.appendChild(dom.createTextNode("{aa3c5121-dab2-40e2-81ca-7ea25febc110}"))
        ta_desc.appendChild(elem)

        elem = dom.createElement("em:minVersion")
        elem.appendChild(dom.createTextNode("26.0"))
        ta_desc.appendChild(elem)

        elem = dom.createElement("em:maxVersion")
        elem.appendChild(dom.createTextNode("30.0a1"))
        ta_desc.appendChild(elem)

    return manifest

if __name__ == "__main__":
    print "Running smoke test."
    root = os.path.join(os.path.dirname(__file__), '../../app-extension')
    manifest = gen_manifest(root, {'name': 'test extension'},
                            'fakeid', 'http://foo.com/update.rdf')
    update = RDFUpdate()
    update.add(manifest, "https://foo.com/foo.xpi")
    exercise_str = str(manifest) + str(update)
    for tagname in ["em:targetApplication", "em:version", "em:id"]:
        if not len(update.dom.getElementsByTagName(tagname)):
            raise Exception("tag does not exist: %s" % tagname)
        if not update.dom.getElementsByTagName(tagname)[0].firstChild:
            raise Exception("tag has no children: %s" % tagname)
    print "Success!"
