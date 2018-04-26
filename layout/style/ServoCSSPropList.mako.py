# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
%>

[
    % for prop in sorted(data.longhands, key=order_key):
    (
        "${prop.name}",
        % if prop.name == "float":
        "CssFloat",
        % elif prop.name.startswith("-x-"):
        "${prop.camel_case[1:]}",
        % else:
        "${prop.camel_case}",
        % endif
        "${prop.ident}",
        [${flags(prop)}],
        ${pref(prop)},
        "longhand",
    ),
    % endfor

    % for prop in sorted(data.shorthands, key=order_key):
    (
        "${prop.name}",
        "${prop.camel_case}",
        "${prop.ident}",
        [${flags(prop)}],
        ${pref(prop)},
        "shorthand",
    ),
    % endfor

    % for prop in sorted(data.all_aliases(), key=lambda x: x.name):
    (
        "${prop.name}",
        "${prop.camel_case}",
        ("${prop.ident}", "${prop.original.ident}"),
        [],
        ${pref(prop)},
        "alias",
    ),
    % endfor
]
