var lastElement;

function openContextMenuFor(element, shiftkey, waitForSpellCheck) {
    // Context menu should be closed before we open it again.
    is(SpecialPowers.wrap(contextMenu).state, "closed", "checking if popup is closed");

    if (lastElement)
      lastElement.blur();
    element.focus();

    // Some elements need time to focus and spellcheck before any tests are
    // run on them.
    function actuallyOpenContextMenuFor() {
      lastElement = element;
      var eventDetails = { type : "contextmenu", button : 2, shiftKey : shiftkey };
      synthesizeMouse(element, 2, 2, eventDetails, element.ownerDocument.defaultView);
    }

    if (waitForSpellCheck)
      onSpellCheck(element, actuallyOpenContextMenuFor);
    else
      actuallyOpenContextMenuFor();
}

function closeContextMenu() {
    contextMenu.hidePopup();
}

function getVisibleMenuItems(aMenu, aData) {
    var items = [];
    var accessKeys = {};
    for (var i = 0; i < aMenu.childNodes.length; i++) {
        var item = aMenu.childNodes[i];
        if (item.hidden)
            continue;

        var key = item.accessKey;
        if (key)
            key = key.toLowerCase();

        var isGenerated = item.hasAttribute("generateditemid");

        if (item.nodeName == "menuitem") {
            var isSpellSuggestion = item.className == "spell-suggestion";
            if (isSpellSuggestion) {
              is(item.id, "", "child menuitem #" + i + " is a spelling suggestion");
            } else if (isGenerated) {
              is(item.id, "", "child menuitem #" + i + " is a generated item");
            } else {
              ok(item.id, "child menuitem #" + i + " has an ID");
            }
            var label = item.getAttribute("label");
            ok(label.length, "menuitem " + item.id + " has a label");
            if (isSpellSuggestion) {
              is(key, "", "Spell suggestions shouldn't have an access key");
              items.push("*" + label);
            } else if (isGenerated) {
              items.push("+" + label);
            } else if (item.id.indexOf("spell-check-dictionary-") != 0 &&
                       item.id != "spell-no-suggestions") {
              ok(key, "menuitem " + item.id + " has an access key");
              if (accessKeys[key])
                  ok(false, "menuitem " + item.id + " has same accesskey as " + accessKeys[key]);
              else
                  accessKeys[key] = item.id;
            }
            if (!isSpellSuggestion && !isGenerated) {
              items.push(item.id);
            }
            if (isGenerated) {
              var p = {};
              p.type = item.getAttribute("type");
              p.icon = item.getAttribute("image");
              p.checked = item.hasAttribute("checked");
              p.disabled = item.hasAttribute("disabled");
              items.push(p);
            } else {
              items.push(!item.disabled);
            }
        } else if (item.nodeName == "menuseparator") {
            ok(true, "--- seperator id is " + item.id);
            items.push("---");
            items.push(null);
        } else if (item.nodeName == "menu") {
            if (isGenerated) {
                item.id = "generated-submenu-" + aData.generatedSubmenuId++;
            }
            ok(item.id, "child menu #" + i + " has an ID");
            if (!isGenerated) {
                ok(key, "menu has an access key");
                if (accessKeys[key])
                    ok(false, "menu " + item.id + " has same accesskey as " + accessKeys[key]);
                else
                    accessKeys[key] = item.id;
            }
            items.push(item.id);
            items.push(!item.disabled);
            // Add a dummy item to that the indexes in checkMenu are the same
            // for expectedItems and actualItems.
            items.push([]);
            items.push(null);
        } else {
            ok(false, "child #" + i + " of menu ID " + aMenu.id +
                      " has an unknown type (" + item.nodeName + ")");
        }
    }
    return items;
}

function checkContextMenu(expectedItems) {
    is(contextMenu.state, "open", "checking if popup is open");
    var data = { generatedSubmenuId: 1 };
    checkMenu(contextMenu, expectedItems, data);
}

/*
 * checkMenu - checks to see if the specified <menupopup> contains the
 * expected items and state.
 * expectedItems is a array of (1) item IDs and (2) a boolean specifying if
 * the item is enabled or not (or null to ignore it). Submenus can be checked
 * by providing a nested array entry after the expected <menu> ID.
 * For example: ["blah", true,              // item enabled
 *               "submenu", null,           // submenu
 *                   ["sub1", true,         // submenu contents
 *                    "sub2", false], null, // submenu contents
 *               "lol", false]              // item disabled
 *
 */
function checkMenu(menu, expectedItems, data) {
    var actualItems = getVisibleMenuItems(menu, data);
    //ok(false, "Items are: " + actualItems);
    for (var i = 0; i < expectedItems.length; i+=2) {
        var actualItem   = actualItems[i];
        var actualEnabled = actualItems[i + 1];
        var expectedItem = expectedItems[i];
        var expectedEnabled = expectedItems[i + 1];
        if (expectedItem instanceof Array) {
            ok(true, "Checking submenu...");
            var menuID = expectedItems[i - 2]; // The last item was the menu ID.
            var submenu = menu.getElementsByAttribute("id", menuID)[0];
            ok(submenu, "got a submenu element of id='" + menuID + "'");
            if (submenu) {
              is(submenu.nodeName, "menu", "submenu element of id='" + menuID +
                                           "' has expected nodeName");
              checkMenu(submenu.menupopup, expectedItem, data);
            }
        } else {
            is(actualItem, expectedItem,
               "checking item #" + i/2 + " (" + expectedItem + ") name");

            if (typeof expectedEnabled == "object" && expectedEnabled != null ||
                typeof actualEnabled == "object" && actualEnabled != null) {

                ok(!(actualEnabled == null), "actualEnabled is not null");
                ok(!(expectedEnabled == null), "expectedEnabled is not null");
                is(typeof actualEnabled, typeof expectedEnabled, "checking types");

                if (typeof actualEnabled != typeof expectedEnabled ||
                    actualEnabled == null || expectedEnabled == null)
                  continue;

                is(actualEnabled.type, expectedEnabled.type,
                   "checking item #" + i/2 + " (" + expectedItem + ") type attr value");
                var icon = actualEnabled.icon;
                if (icon) {
                  var tmp = "";
                  var j = icon.length - 1;
                  while (j && icon[j] != "/") {
                    tmp = icon[j--] + tmp;
                  }
                  icon = tmp;
                }
                is(icon, expectedEnabled.icon,
                   "checking item #" + i/2 + " (" + expectedItem + ") icon attr value");
                is(actualEnabled.checked, expectedEnabled.checked,
                   "checking item #" + i/2 + " (" + expectedItem + ") has checked attr");
                is(actualEnabled.disabled, expectedEnabled.disabled,
                   "checking item #" + i/2 + " (" + expectedItem + ") has disabled attr");
            } else if (expectedEnabled != null)
                is(actualEnabled, expectedEnabled,
                   "checking item #" + i/2 + " (" + expectedItem + ") enabled state");
        }
    }
    // Could find unexpected extra items at the end...
    is(actualItems.length, expectedItems.length, "checking expected number of menu entries");
}
