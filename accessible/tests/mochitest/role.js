////////////////////////////////////////////////////////////////////////////////
// Role constants

const ROLE_ALERT = nsIAccessibleRole.ROLE_ALERT;
const ROLE_ANIMATION = nsIAccessibleRole.ROLE_ANIMATION;
const ROLE_APPLICATION = nsIAccessibleRole.ROLE_APPLICATION;
const ROLE_APP_ROOT = nsIAccessibleRole.ROLE_APP_ROOT;
const ROLE_AUTOCOMPLETE = nsIAccessibleRole.ROLE_AUTOCOMPLETE;
const ROLE_BUTTONDROPDOWNGRID = nsIAccessibleRole.ROLE_BUTTONDROPDOWNGRID;
const ROLE_CANVAS = nsIAccessibleRole.ROLE_CANVAS;
const ROLE_CAPTION = nsIAccessibleRole.ROLE_CAPTION;
const ROLE_CELL = nsIAccessibleRole.ROLE_CELL;
const ROLE_CHECKBUTTON = nsIAccessibleRole.ROLE_CHECKBUTTON;
const ROLE_CHROME_WINDOW = nsIAccessibleRole.ROLE_CHROME_WINDOW;
const ROLE_COMBOBOX = nsIAccessibleRole.ROLE_COMBOBOX;
const ROLE_COMBOBOX_LIST = nsIAccessibleRole.ROLE_COMBOBOX_LIST;
const ROLE_COMBOBOX_OPTION = nsIAccessibleRole.ROLE_COMBOBOX_OPTION;
const ROLE_COLUMNHEADER = nsIAccessibleRole.ROLE_COLUMNHEADER;
const ROLE_DIALOG = nsIAccessibleRole.ROLE_DIALOG;
const ROLE_DOCUMENT = nsIAccessibleRole.ROLE_DOCUMENT;
const ROLE_EMBEDDED_OBJECT = nsIAccessibleRole.ROLE_EMBEDDED_OBJECT;
const ROLE_ENTRY = nsIAccessibleRole.ROLE_ENTRY;
const ROLE_FOOTER = nsIAccessibleRole.ROLE_FOOTER;
const ROLE_FLAT_EQUATION = nsIAccessibleRole.ROLE_FLAT_EQUATION;
const ROLE_FORM = nsIAccessibleRole.ROLE_FORM;
const ROLE_GRAPHIC = nsIAccessibleRole.ROLE_GRAPHIC;
const ROLE_GRID_CELL = nsIAccessibleRole.ROLE_GRID_CELL;
const ROLE_GROUPING = nsIAccessibleRole.ROLE_GROUPING;
const ROLE_HEADER = nsIAccessibleRole.ROLE_HEADER;
const ROLE_HEADING = nsIAccessibleRole.ROLE_HEADING;
const ROLE_IMAGE_MAP = nsIAccessibleRole.ROLE_IMAGE_MAP;
const ROLE_INTERNAL_FRAME = nsIAccessibleRole.ROLE_INTERNAL_FRAME;
const ROLE_LABEL = nsIAccessibleRole.ROLE_LABEL;
const ROLE_LINK = nsIAccessibleRole.ROLE_LINK;
const ROLE_LIST = nsIAccessibleRole.ROLE_LIST;
const ROLE_LISTBOX = nsIAccessibleRole.ROLE_LISTBOX;
const ROLE_LISTITEM = nsIAccessibleRole.ROLE_LISTITEM;
const ROLE_MENUITEM = nsIAccessibleRole.ROLE_MENUITEM;
const ROLE_MENUPOPUP = nsIAccessibleRole.ROLE_MENUPOPUP;
const ROLE_NOTHING = nsIAccessibleRole.ROLE_NOTHING;
const ROLE_NOTE = nsIAccessibleRole.ROLE_NOTE;
const ROLE_OPTION = nsIAccessibleRole.ROLE_OPTION;
const ROLE_OUTLINE = nsIAccessibleRole.ROLE_OUTLINE;
const ROLE_OUTLINEITEM = nsIAccessibleRole.ROLE_OUTLINEITEM;
const ROLE_PAGETAB = nsIAccessibleRole.ROLE_PAGETAB;
const ROLE_PAGETABLIST = nsIAccessibleRole.ROLE_PAGETABLIST;
const ROLE_PANE = nsIAccessibleRole.ROLE_PANE;
const ROLE_PARAGRAPH = nsIAccessibleRole.ROLE_PARAGRAPH;
const ROLE_PARENT_MENUITEM = nsIAccessibleRole.ROLE_PARENT_MENUITEM;
const ROLE_PASSWORD_TEXT = nsIAccessibleRole.ROLE_PASSWORD_TEXT;
const ROLE_PROGRESSBAR = nsIAccessibleRole.ROLE_PROGRESSBAR;
const ROLE_PROPERTYPAGE = nsIAccessibleRole.ROLE_PROPERTYPAGE;
const ROLE_PUSHBUTTON = nsIAccessibleRole.ROLE_PUSHBUTTON;
const ROLE_RADIOBUTTON = nsIAccessibleRole.ROLE_RADIOBUTTON;
const ROLE_ROW = nsIAccessibleRole.ROLE_ROW;
const ROLE_ROWHEADER = nsIAccessibleRole.ROLE_ROWHEADER;
const ROLE_SCROLLBAR = nsIAccessibleRole.ROLE_SCROLLBAR;
const ROLE_SECTION = nsIAccessibleRole.ROLE_SECTION;
const ROLE_SEPARATOR = nsIAccessibleRole.ROLE_SEPARATOR;
const ROLE_SLIDER = nsIAccessibleRole.ROLE_SLIDER;
const ROLE_STATICTEXT = nsIAccessibleRole.ROLE_STATICTEXT;
const ROLE_TABLE = nsIAccessibleRole.ROLE_TABLE;
const ROLE_TEXT_CONTAINER = nsIAccessibleRole.ROLE_TEXT_CONTAINER;
const ROLE_TEXT_LEAF = nsIAccessibleRole.ROLE_TEXT_LEAF;
const ROLE_TOGGLE_BUTTON = nsIAccessibleRole.ROLE_TOGGLE_BUTTON;
const ROLE_TOOLBAR = nsIAccessibleRole.ROLE_TOOLBAR;
const ROLE_TOOLTIP = nsIAccessibleRole.ROLE_TOOLTIP;
const ROLE_TREE_TABLE = nsIAccessibleRole.ROLE_TREE_TABLE;
const ROLE_WHITESPACE = nsIAccessibleRole.ROLE_WHITESPACE;

////////////////////////////////////////////////////////////////////////////////
// Public methods

/**
 * Test that the role of the given accessible is the role passed in.
 *
 * @param aAccOrElmOrID  the accessible, DOM element or ID to be tested.
 * @param aRole  The role that is to be expected.
 */
function testRole(aAccOrElmOrID, aRole)
{
  var role = getRole(aAccOrElmOrID);
  is(role, aRole, "Wrong role for " + prettyName(aAccOrElmOrID) + "!");
}

/**
 * Return the role of the given accessible. Return -1 if accessible could not
 * be retrieved.
 *
 * @param aAccOrElmOrID  [in] The accessible, DOM element or element ID the
 *                       accessible role is being requested for.
 */
function getRole(aAccOrElmOrID)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return -1;

  var role = -1;
  try {
    role = acc.role;
  } catch(e) {
    ok(false, "Role for " + aAccOrElmOrID + " could not be retrieved!");
  }

  return role;
}

/**
 * Analogy of SimpleTest.is function used to check the role.
 */
function isRole(aIdentifier, aRole, aMsg)
{
  var role = getRole(aIdentifier);
  if (role == - 1)
    return;

  if (role == aRole) {
    ok(true, aMsg);
    return;
  }

  var got = roleToString(role);
  var expected = roleToString(aRole);

  ok(false, aMsg + "got '" + got + "', expected '" + expected + "'");
}
