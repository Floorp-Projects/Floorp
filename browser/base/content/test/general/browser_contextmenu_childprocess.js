/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const gBaseURL = "https://example.com/browser/browser/base/content/test/general/";

add_task(function *() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, gBaseURL + "subtst_contextmenu.html");

  let contextMenu = document.getElementById("contentAreaContextMenu");

  // Get the point of the element with the page menu (test-pagemenu) and
  // synthesize a right mouse click there.
  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  yield BrowserTestUtils.synthesizeMouse("#test-pagemenu", 5, 5, { type : "contextmenu", button : 2 }, tab.linkedBrowser);
  yield popupShownPromise;

  checkMenu(contextMenu);

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  yield popupHiddenPromise;

  yield BrowserTestUtils.removeTab(tab);
});

function checkItems(menuitem, arr)
{
  for (let i = 0; i < arr.length; i += 2) {
    let str = arr[i];
    let details = arr[i + 1];
    if (str == "---") {
      is(menuitem.localName, "menuseparator", "menuseparator");
    }
    else if ("children" in details) {
      is(menuitem.localName, "menu", "submenu");
      is(menuitem.getAttribute("label"), str, str + " label");
      checkItems(menuitem.firstChild.firstChild, details.children);
    }
    else {
      is(menuitem.localName, "menuitem", str + " menuitem");

      is(menuitem.getAttribute("label"), str, str + " label");
      is(menuitem.getAttribute("type"), details.type, str + " type");
      is(menuitem.getAttribute("image"), details.icon ? gBaseURL + details.icon : "", str + " icon");

      if (details.checked)
        is(menuitem.getAttribute("checked"), "true", str + " checked");
      else
        ok(!menuitem.hasAttribute("checked"), str + " checked");

      if (details.disabled)
        is(menuitem.getAttribute("disabled"), "true", str + " disabled");
      else
        ok(!menuitem.hasAttribute("disabled"), str + " disabled");
    }

    menuitem = menuitem.nextSibling;
  }
}

function checkMenu(contextMenu)
{
  let items = [ "Plain item",          {type: "", icon: "", checked: false, disabled: false},
                "Disabled item",       {type: "", icon: "", checked: false, disabled: true},
                "Item w/ textContent", {type: "", icon: "", checked: false, disabled: false},
                "---",                  null,
                "Checkbox",            {type: "checkbox", icon: "", checked: true, disabled: false},
                "---",                  null,
                "Radio1",              {type: "checkbox", icon: "", checked: true, disabled: false},
                "Radio2",              {type: "checkbox", icon: "", checked: false, disabled: false},
                "Radio3",              {type: "checkbox", icon: "", checked: false, disabled: false},
                "---",                  null,
                "Item w/ icon",        {type: "", icon: "favicon.ico", checked: false, disabled: false},
                "Item w/ bad icon",    {type: "", icon: "", checked: false, disabled: false},
                "---",                  null,
                "Submenu",  { children:
                  ["Radio1",             {type: "checkbox", icon: "", checked: false, disabled: false},
                   "Radio2",             {type: "checkbox", icon: "", checked: true, disabled: false},
                   "Radio3",             {type: "checkbox", icon: "", checked: false, disabled: false},
                   "---",                 null,
                   "Checkbox",           {type: "checkbox", icon: "", checked: false, disabled: false}] }
               ];
  checkItems(contextMenu.childNodes[2], items);
}
