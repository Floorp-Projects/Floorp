# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from xml.dom.minidom import Document

VALID_PREF_TYPES = ['bool', 'boolint', 'integer', 'string', 'color', 'file',
                    'directory', 'control', 'menulist', 'radio']

class Error(Exception):
    pass

class BadPrefTypeError(Error):
    pass

class MissingPrefAttr(Error):
    pass

def validate_prefs(options):
    for pref in options:
        # Make sure there is a 'title'
        if ("title" not in pref):
            raise MissingPrefAttr("The '%s' pref requires a 'title'" % (pref["name"]))

        # Make sure that the pref type is a valid inline pref type
        if (pref["type"] not in VALID_PREF_TYPES):
            raise BadPrefTypeError('%s is not a valid inline pref type' % (pref["type"]))

        # Make sure the 'control' type has a 'label'
        if (pref["type"] == "control"):
            if ("label" not in pref):
                raise MissingPrefAttr("The 'control' inline pref type requires a 'label'")

        # Make sure the 'menulist' type has a 'menulist'
        if (pref["type"] == "menulist" or pref["type"] == "radio"):
            if ("options" not in pref):
                raise MissingPrefAttr("The 'menulist' and the 'radio' inline pref types requires a 'options'")

            # Make sure each option has a 'value' and a 'label'
            for item in pref["options"]:
                if ("value" not in item):
                    raise MissingPrefAttr("'options' requires a 'value'")
                if ("label" not in item):
                    raise MissingPrefAttr("'options' requires a 'label'")

        # TODO: Check that pref["type"] matches default value type

def parse_options(options, jetpack_id):
    doc = Document()
    root = doc.createElement("vbox")
    root.setAttribute("xmlns", "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")
    doc.appendChild(root)

    for pref in options:
        setting = doc.createElement("setting")
        setting.setAttribute("pref-name", pref["name"])
        setting.setAttribute("data-jetpack-id", jetpack_id)
        setting.setAttribute("pref", "extensions." + jetpack_id + "." + pref["name"])
        setting.setAttribute("type", pref["type"])
        setting.setAttribute("title", pref["title"])

        if ("description" in pref):
            setting.appendChild(doc.createTextNode(pref["description"]))

        if (pref["type"] == "control"):
            button = doc.createElement("button")
            button.setAttribute("pref-name", pref["name"])
            button.setAttribute("data-jetpack-id", jetpack_id)
            button.setAttribute("label", pref["label"])
            button.setAttribute("oncommand", "Services.obs.notifyObservers(null, '" +
                                              jetpack_id + "-cmdPressed', '" +
                                              pref["name"] + "');");
            setting.appendChild(button)
        elif (pref["type"] == "boolint"):
            setting.setAttribute("on", pref["on"])
            setting.setAttribute("off", pref["off"])
        elif (pref["type"] == "menulist"):
            menulist = doc.createElement("menulist")
            menupopup = doc.createElement("menupopup")
            for item in pref["options"]:
                menuitem = doc.createElement("menuitem")
                menuitem.setAttribute("value", item["value"])
                menuitem.setAttribute("label", item["label"])
                menupopup.appendChild(menuitem)
            menulist.appendChild(menupopup)
            setting.appendChild(menulist)
        elif (pref["type"] == "radio"):
            radiogroup = doc.createElement("radiogroup")
            for item in pref["options"]:
                radio = doc.createElement("radio")
                radio.setAttribute("value", item["value"])
                radio.setAttribute("label", item["label"])
                radiogroup.appendChild(radio)
            setting.appendChild(radiogroup)

        root.appendChild(setting)

    return doc.toprettyxml(indent="  ")
