# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

def _assign_slots(obj, args):
    for i, attr in enumerate(obj.__slots__):
        setattr(obj, attr, args[i])


class Longhand(object):
    __slots__ = ["name", "method", "id", "rules", "flags", "pref", "aliases"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "longhand"


class Shorthand(object):
    __slots__ = ["name", "method", "id", "rules", "flags", "pref", "subprops", "aliases"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "shorthand"


class Alias(object):
    __slots__ = ["name", "method", "alias_id", "prop_id", "rules", "flags", "pref"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "alias"

<%!
# See bug 1454823 for situation of internal flag.
def is_internal(prop):
    # A property which is not controlled by pref and not enabled in
    # content by default is an internal property.
    return not prop.gecko_pref and not prop.enabled_in_content()

def method(prop):
    if prop.name == "float":
        return "CssFloat"
    if prop.name.startswith("-x-"):
        return prop.camel_case[1:]
    return prop.camel_case

# TODO(emilio): Get this to zero.
LONGHANDS_NOT_SERIALIZED_WITH_SERVO = [
    # Servo serializes one value when both are the same, a few tests expect two.
    "border-spacing",

    # These resolve auto to zero in a few cases, but not all.
    "max-height",
    "max-width",
    "min-height",
    "min-width",

    # resistfingerprinting stuff.
    "-moz-osx-font-smoothing",

    # Layout dependent.
    "width",
    "height",
    "grid-template-rows",
    "grid-template-columns",
    "perspective-origin",
    "transform-origin",
    "transform",
    "top",
    "right",
    "bottom",
    "left",
    "border-top-width",
    "border-right-width",
    "border-bottom-width",
    "border-left-width",
    "margin-top",
    "margin-right",
    "margin-bottom",
    "margin-left",
    "padding-top",
    "padding-right",
    "padding-bottom",
    "padding-left",
]

def serialized_by_servo(prop):
    if prop.type() == "shorthand" or prop.type() == "alias":
        return True
    # Keywords are all fine, except -moz-osx-font-smoothing, which does
    # resistfingerprinting stuff.
    if prop.keyword and prop.name != "-moz-osx-font-smoothing":
        return True
    return prop.name not in LONGHANDS_NOT_SERIALIZED_WITH_SERVO

def exposed_on_getcs(prop):
    if "Style" not in prop.rule_types_allowed_names():
        return False
    if is_internal(prop):
        return False
    return True

def rules(prop):
    return ", ".join('"{}"'.format(rule) for rule in prop.rule_types_allowed_names())

RUST_TO_CPP_FLAGS = {
  "CAN_ANIMATE_ON_COMPOSITOR": "CanAnimateOnCompositor",
  "AFFECTS_LAYOUT": "AffectsLayout",
  "AFFECTS_PAINT": "AffectsPaint",
  "AFFECTS_OVERFLOW": "AffectsOverflow",
}

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
    for (k, v) in RUST_TO_CPP_FLAGS.items():
        if k in prop.flags:
            result.append(v)
    if exposed_on_getcs(prop):
        result.append("ExposedOnGetCS")
        if prop.type() == "shorthand" and "SHORTHAND_IN_GETCS" in prop.flags:
            result.append("ShorthandUnconditionallyExposedOnGetCS")
        if serialized_by_servo(prop):
            result.append("SerializedByServo")
    if prop.type() == "longhand" and prop.logical:
        result.append("IsLogical")
    return ", ".join('"{}"'.format(flag) for flag in result)

def pref(prop):
    if prop.gecko_pref:
        return '"' + prop.gecko_pref + '"'
    return '""'

def sub_properties(prop):
    return ", ".join('"{}"'.format(p.ident) for p in prop.sub_properties)

def aliases(prop):
    return ", ".join('"{}"'.format(p.ident) for p in prop.aliases)
%>

data = {
% for prop in data.longhands:
    "${prop.ident}": Longhand("${prop.name}", "${method(prop)}", "${prop.ident}", [${rules(prop)}], [${flags(prop)}], ${pref(prop)}, [${aliases(prop)}]),
% endfor

% for prop in data.shorthands:
    "${prop.ident}": Shorthand("${prop.name}", "${prop.camel_case}", "${prop.ident}", [${rules(prop)}], [${flags(prop)}], ${pref(prop)}, [${sub_properties(prop)}], [${aliases(prop)}]),
% endfor

% for prop in data.all_aliases():
    "${prop.ident}": Alias("${prop.name}", "${prop.camel_case}", "${prop.ident}", "${prop.original.ident}", [${rules(prop)}], [${flags(prop)}], ${pref(prop)}),
% endfor
}
