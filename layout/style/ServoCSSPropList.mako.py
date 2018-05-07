# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

def _assign_slots(obj, args):
    for i, attr in enumerate(obj.__slots__):
        setattr(obj, attr, args[i])


class Longhand(object):
    __slots__ = ["name", "method", "id", "flags", "pref"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "longhand"


class Shorthand(object):
    __slots__ = ["name", "method", "id", "flags", "pref", "subprops"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "shorthand"


class Alias(object):
    __slots__ = ["name", "method", "alias_id", "prop_id", "flags", "pref"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "alias"

<%!
# nsCSSPropertyID of longhands and shorthands is ordered alphabetically
# with vendor prefixes removed. Note that aliases use their alias name
# as order key directly because they may be duplicate without prefix.
def order_key(prop):
    if prop.name.startswith("-"):
        return prop.name[prop.name.find("-", 1) + 1:]
    return prop.name

# See bug 1454823 for situation of internal flag.
def is_internal(prop):
    # A property which is not controlled by pref and not enabled in
    # content by default is an internal property.
    if not prop.gecko_pref and not prop.enabled_in_content():
        return True
    # There are some special cases we may want to remove eventually.
    OTHER_INTERNALS = [
        "-moz-context-properties",
        "-moz-control-character-visibility",
        "-moz-window-opacity",
        "-moz-window-transform",
        "-moz-window-transform-origin",
    ]
    return prop.name in OTHER_INTERNALS

def method(prop):
    if prop.name == "float":
        return "CssFloat"
    if prop.name.startswith("-x-"):
        return prop.camel_case[1:]
    return prop.camel_case

def flags(prop):
    result = []
    if prop.explicitly_enabled_in_chrome():
        result.append("EnabledInUASheetsAndChrome")
    elif prop.explicitly_enabled_in_ua_sheets():
        result.append("EnabledInUASheets")
    if is_internal(prop):
        result.append("Internal")
    if prop.enabled_in == "":
        result.append("Inaccessible")
    if "GETCS_NEEDS_LAYOUT_FLUSH" in prop.flags:
        result.append("GetCSNeedsLayoutFlush")
    if "CAN_ANIMATE_ON_COMPOSITOR" in prop.flags:
        result.append("CanAnimateOnCompositor")
    return ", ".join('"CSSPropFlags::{}"'.format(flag) for flag in result)

def pref(prop):
    if prop.gecko_pref:
        return '"' + prop.gecko_pref + '"'
    return '""'

def sub_properties(prop):
    return ", ".join('"{}"'.format(p.ident) for p in prop.sub_properties)
%>

data = [
    % for prop in sorted(data.longhands, key=order_key):
    Longhand("${prop.name}", "${method(prop)}", "${prop.ident}", [${flags(prop)}], ${pref(prop)}),
    % endfor

    % for prop in sorted(data.shorthands, key=order_key):
    Shorthand("${prop.name}", "${prop.camel_case}", "${prop.ident}", [${flags(prop)}], ${pref(prop)},
              [${sub_properties(prop)}]),
    % endfor

    % for prop in sorted(data.all_aliases(), key=lambda x: x.name):
    Alias("${prop.name}", "${prop.camel_case}", "${prop.ident}", "${prop.original.ident}", [], ${pref(prop)}),
    % endfor
]
