#include "nsCOMPtr.h"
#include "nsXBLEventHandler.h"
#include "nsIContent.h"
#include "nsIDOMKeyEvent.h"

nsXBLEventHandler::nsXBLEventHandler(nsIContent* aBoundElement, nsIContent* aHandlerElement)
{
  NS_INIT_REFCNT();
  mBoundElement = aBoundElement;
  mHandlerElement = aHandlerElement;
}

nsXBLEventHandler::~nsXBLEventHandler()
{
}

NS_IMPL_ISUPPORTS2(nsXBLEventHandler, nsIDOMKeyListener, nsIDOMMouseListener)

nsresult nsXBLEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  // Nothing to do.
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyUp(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler();
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler();
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler();
  return NS_OK;
}
   
nsresult nsXBLEventHandler::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler();
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseUp(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler();
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler();
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler();
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOver(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler();
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOut(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler();
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

PRBool 
nsXBLEventHandler::KeyEventMatched(nsIDOMKeyEvent* aKeyEvent)
{
  nsAutoString trueString = "true";
  nsAutoString falseString = "false";

  return PR_TRUE;
}

PRBool 
nsXBLEventHandler::MouseEventMatched(nsIDOMUIEvent* aMouseEvent)
{
  nsAutoString trueString = "true";
  nsAutoString falseString = "false";

  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLEventHandler::ExecuteHandler()
{
  return NS_OK;
}

/// Helpers that are relegated to the end of the file /////////////////////////////

enum {
    VK_CANCEL = 3,
    VK_BACK = 8,
    VK_TAB = 9,
    VK_CLEAR = 12,
    VK_RETURN = 13,
    VK_ENTER = 14,
    VK_SHIFT = 16,
    VK_CONTROL = 17,
    VK_ALT = 18,
    VK_PAUSE = 19,
    VK_CAPS_LOCK = 20,
    VK_ESCAPE = 27,
    VK_SPACE = 32,
    VK_PAGE_UP = 33,
    VK_PAGE_DOWN = 34,
    VK_END = 35,
    VK_HOME = 36,
    VK_LEFT = 37,
    VK_UP = 38,
    VK_RIGHT = 39,
    VK_DOWN = 40,
    VK_PRINTSCREEN = 44,
    VK_INSERT = 45,
    VK_DELETE = 46,
    VK_0 = 48,
    VK_1 = 49,
    VK_2 = 50,
    VK_3 = 51,
    VK_4 = 52,
    VK_5 = 53,
    VK_6 = 54,
    VK_7 = 55,
    VK_8 = 56,
    VK_9 = 57,
    VK_SEMICOLON = 59,
    VK_EQUALS = 61,
    VK_A = 65,
    VK_B = 66,
    VK_C = 67,
    VK_D = 68,
    VK_E = 69,
    VK_F = 70,
    VK_G = 71,
    VK_H = 72,
    VK_I = 73,
    VK_J = 74,
    VK_K = 75,
    VK_L = 76,
    VK_M = 77,
    VK_N = 78,
    VK_O = 79,
    VK_P = 80,
    VK_Q = 81,
    VK_R = 82,
    VK_S = 83,
    VK_T = 84,
    VK_U = 85,
    VK_V = 86,
    VK_W = 87,
    VK_X = 88,
    VK_Y = 89,
    VK_Z = 90,
    VK_NUMPAD0 = 96,
    VK_NUMPAD1 = 97,
    VK_NUMPAD2 = 98,
    VK_NUMPAD3 = 99,
    VK_NUMPAD4 = 100,
    VK_NUMPAD5 = 101,
    VK_NUMPAD6 = 102,
    VK_NUMPAD7 = 103,
    VK_NUMPAD8 = 104,
    VK_NUMPAD9 = 105,
    VK_MULTIPLY = 106,
    VK_ADD = 107,
    VK_SEPARATOR = 108,
    VK_SUBTRACT = 109,
    VK_DECIMAL = 110,
    VK_DIVIDE = 111,
    VK_F1 = 112,
    VK_F2 = 113,
    VK_F3 = 114,
    VK_F4 = 115,
    VK_F5 = 116,
    VK_F6 = 117,
    VK_F7 = 118,
    VK_F8 = 119,
    VK_F9 = 120,
    VK_F10 = 121,
    VK_F11 = 122,
    VK_F12 = 123,
    VK_F13 = 124,
    VK_F14 = 125,
    VK_F15 = 126,
    VK_F16 = 127,
    VK_F17 = 128,
    VK_F18 = 129,
    VK_F19 = 130,
    VK_F20 = 131,
    VK_F21 = 132,
    VK_F22 = 133,
    VK_F23 = 134,
    VK_F24 = 135,
    VK_NUM_LOCK = 144,
    VK_SCROLL_LOCK = 145,
    VK_COMMA = 188,
    VK_PERIOD = 190,
    VK_SLASH = 191,
    VK_BACK_QUOTE = 192,
    VK_OPEN_BRACKET = 219,
    VK_BACK_SLASH = 220,
    VK_CLOSE_BRACKET = 221,
    VK_QUOTE = 222
};

PRBool nsXBLEventHandler::IsMatchingKeyCode(const PRUint32 aChar, const nsString& aKeyName)
{
  PRBool ret = PR_FALSE;

  switch(aChar) {
    case VK_CANCEL:
      if(aKeyName.EqualsIgnoreCase("VK_CANCEL"))
        ret = PR_TRUE;
        break;
    case VK_BACK:
      if(aKeyName.EqualsIgnoreCase("VK_BACK"))
        ret = PR_TRUE;
        break;
    case VK_TAB:
      if(aKeyName.EqualsIgnoreCase("VK_TAB"))
        ret = PR_TRUE;
        break;
    case VK_CLEAR:
      if(aKeyName.EqualsIgnoreCase("VK_CLEAR"))
        ret = PR_TRUE;
        break;
    case VK_RETURN:
      if(aKeyName.EqualsIgnoreCase("VK_RETURN"))
        ret = PR_TRUE;
        break;
    case VK_ENTER:
      if(aKeyName.EqualsIgnoreCase("VK_ENTER"))
        ret = PR_TRUE;
        break;
    case VK_SHIFT:
      if(aKeyName.EqualsIgnoreCase("VK_SHIFT"))
        ret = PR_TRUE;
        break;
    case VK_CONTROL:
      if(aKeyName.EqualsIgnoreCase("VK_CONTROL"))
        ret = PR_TRUE;
        break;
    case VK_ALT:
      if(aKeyName.EqualsIgnoreCase("VK_ALT"))
        ret = PR_TRUE;
        break;
    case VK_PAUSE:
      if(aKeyName.EqualsIgnoreCase("VK_PAUSE"))
        ret = PR_TRUE;
        break;
    case VK_CAPS_LOCK:
      if(aKeyName.EqualsIgnoreCase("VK_CAPS_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_ESCAPE:
      if(aKeyName.EqualsIgnoreCase("VK_ESCAPE"))
        ret = PR_TRUE;
        break;
    case VK_SPACE:
      if(aKeyName.EqualsIgnoreCase("VK_SPACE"))
        ret = PR_TRUE;
        break;
    case VK_PAGE_UP:
      if(aKeyName.EqualsIgnoreCase("VK_PAGE_UP"))
        ret = PR_TRUE;
        break;
    case VK_PAGE_DOWN:
      if(aKeyName.EqualsIgnoreCase("VK_PAGE_DOWN"))
        ret = PR_TRUE;
        break;
    case VK_END:
      if(aKeyName.EqualsIgnoreCase("VK_END"))
        ret = PR_TRUE;
        break;
    case VK_HOME:
      if(aKeyName.EqualsIgnoreCase("VK_HOME"))
        ret = PR_TRUE;
        break;
    case VK_LEFT:
      if(aKeyName.EqualsIgnoreCase("VK_LEFT"))
        ret = PR_TRUE;
        break;
    case VK_UP:
      if(aKeyName.EqualsIgnoreCase("VK_UP"))
        ret = PR_TRUE;
        break;
    case VK_RIGHT:
      if(aKeyName.EqualsIgnoreCase("VK_RIGHT"))
        ret = PR_TRUE;
        break;
    case VK_DOWN:
      if(aKeyName.EqualsIgnoreCase("VK_DOWN"))
        ret = PR_TRUE;
        break;
    case VK_PRINTSCREEN:
      if(aKeyName.EqualsIgnoreCase("VK_PRINTSCREEN"))
        ret = PR_TRUE;
        break;
    case VK_INSERT:
      if(aKeyName.EqualsIgnoreCase("VK_INSERT"))
        ret = PR_TRUE;
        break;
    case VK_DELETE:
      if(aKeyName.EqualsIgnoreCase("VK_DELETE"))
        ret = PR_TRUE;
        break;
    case VK_0:
      if(aKeyName.EqualsIgnoreCase("VK_0"))
        ret = PR_TRUE;
        break;
    case VK_1:
      if(aKeyName.EqualsIgnoreCase("VK_1"))
        ret = PR_TRUE;
        break;
    case VK_2:
      if(aKeyName.EqualsIgnoreCase("VK_2"))
        ret = PR_TRUE;
        break;
    case VK_3:
      if(aKeyName.EqualsIgnoreCase("VK_3"))
        ret = PR_TRUE;
        break;
    case VK_4:
      if(aKeyName.EqualsIgnoreCase("VK_4"))
        ret = PR_TRUE;
        break;
    case VK_5:
      if(aKeyName.EqualsIgnoreCase("VK_5"))
        ret = PR_TRUE;
        break;
    case VK_6:
      if(aKeyName.EqualsIgnoreCase("VK_6"))
        ret = PR_TRUE;
        break;
    case VK_7:
      if(aKeyName.EqualsIgnoreCase("VK_7"))
        ret = PR_TRUE;
        break;
    case VK_8:
      if(aKeyName.EqualsIgnoreCase("VK_8"))
        ret = PR_TRUE;
        break;
    case VK_9:
      if(aKeyName.EqualsIgnoreCase("VK_9"))
        ret = PR_TRUE;
        break;
    case VK_SEMICOLON:
      if(aKeyName.EqualsIgnoreCase("VK_SEMICOLON"))
        ret = PR_TRUE;
        break;
    case VK_EQUALS:
      if(aKeyName.EqualsIgnoreCase("VK_EQUALS"))
        ret = PR_TRUE;
        break;
    case VK_A:
      if(aKeyName.EqualsIgnoreCase("VK_A"))
        ret = PR_TRUE;
        break;
    case VK_B:
      if(aKeyName.EqualsIgnoreCase("VK_B"))
        ret = PR_TRUE;
    break;
    case VK_C:
      if(aKeyName.EqualsIgnoreCase("VK_C"))
        ret = PR_TRUE;
        break;
    case VK_D:
      if(aKeyName.EqualsIgnoreCase("VK_D"))
        ret = PR_TRUE;
        break;
    case VK_E:
      if(aKeyName.EqualsIgnoreCase("VK_E"))
        ret = PR_TRUE;
        break;
    case VK_F:
      if(aKeyName.EqualsIgnoreCase("VK_F"))
        ret = PR_TRUE;
        break;
    case VK_G:
      if(aKeyName.EqualsIgnoreCase("VK_G"))
        ret = PR_TRUE;
        break;
    case VK_H:
      if(aKeyName.EqualsIgnoreCase("VK_H"))
        ret = PR_TRUE;
        break;
    case VK_I:
      if(aKeyName.EqualsIgnoreCase("VK_I"))
        ret = PR_TRUE;
        break;
    case VK_J:
      if(aKeyName.EqualsIgnoreCase("VK_J"))
        ret = PR_TRUE;
        break;
    case VK_K:
      if(aKeyName.EqualsIgnoreCase("VK_K"))
        ret = PR_TRUE;
        break;
    case VK_L:
      if(aKeyName.EqualsIgnoreCase("VK_L"))
        ret = PR_TRUE;
        break;
    case VK_M:
      if(aKeyName.EqualsIgnoreCase("VK_M"))
        ret = PR_TRUE;
        break;
    case VK_N:
      if(aKeyName.EqualsIgnoreCase("VK_N"))
        ret = PR_TRUE;
        break;
    case VK_O:
      if(aKeyName.EqualsIgnoreCase("VK_O"))
        ret = PR_TRUE;
        break;
    case VK_P:
      if(aKeyName.EqualsIgnoreCase("VK_P"))
        ret = PR_TRUE;
        break;
    case VK_Q:
      if(aKeyName.EqualsIgnoreCase("VK_Q"))
        ret = PR_TRUE;
        break;
    case VK_R:
      if(aKeyName.EqualsIgnoreCase("VK_R"))
        ret = PR_TRUE;
        break;
    case VK_S:
      if(aKeyName.EqualsIgnoreCase("VK_S"))
        ret = PR_TRUE;
        break;
    case VK_T:
      if(aKeyName.EqualsIgnoreCase("VK_T"))
        ret = PR_TRUE;
        break;
    case VK_U:
      if(aKeyName.EqualsIgnoreCase("VK_U"))
        ret = PR_TRUE;
        break;
    case VK_V:
      if(aKeyName.EqualsIgnoreCase("VK_V"))
        ret = PR_TRUE;
        break;
    case VK_W:
      if(aKeyName.EqualsIgnoreCase("VK_W"))
        ret = PR_TRUE;
        break;
    case VK_X:
      if(aKeyName.EqualsIgnoreCase("VK_X"))
        ret = PR_TRUE;
        break;
    case VK_Y:
      if(aKeyName.EqualsIgnoreCase("VK_Y"))
        ret = PR_TRUE;
        break;
    case VK_Z:
      if(aKeyName.EqualsIgnoreCase("VK_Z"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD0:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD0"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD1:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD1"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD2:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD2"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD3:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD3"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD4:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD4"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD5:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD5"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD6:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD6"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD7:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD7"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD8:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD8"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD9:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD9"))
        ret = PR_TRUE;
        break;
    case VK_MULTIPLY:
      if(aKeyName.EqualsIgnoreCase("VK_MULTIPLY"))
        ret = PR_TRUE;
        break;
    case VK_ADD:
      if(aKeyName.EqualsIgnoreCase("VK_ADD"))
        ret = PR_TRUE;
        break;
    case VK_SEPARATOR:
      if(aKeyName.EqualsIgnoreCase("VK_SEPARATOR"))
        ret = PR_TRUE;
        break;
    case VK_SUBTRACT:
      if(aKeyName.EqualsIgnoreCase("VK_SUBTRACT"))
        ret = PR_TRUE;
        break;
    case VK_DECIMAL:
      if(aKeyName.EqualsIgnoreCase("VK_DECIMAL"))
        ret = PR_TRUE;
        break;
    case VK_DIVIDE:
      if(aKeyName.EqualsIgnoreCase("VK_DIVIDE"))
        ret = PR_TRUE;
        break;
    case VK_F1:
      if(aKeyName.EqualsIgnoreCase("VK_F1"))
        ret = PR_TRUE;
        break;
    case VK_F2:
      if(aKeyName.EqualsIgnoreCase("VK_F2"))
        ret = PR_TRUE;
        break;
    case VK_F3:
      if(aKeyName.EqualsIgnoreCase("VK_F3"))
        ret = PR_TRUE;
        break;
    case VK_F4:
      if(aKeyName.EqualsIgnoreCase("VK_F4"))
        ret = PR_TRUE;
        break;
    case VK_F5:
      if(aKeyName.EqualsIgnoreCase("VK_F5"))
        ret = PR_TRUE;
        break;
    case VK_F6:
      if(aKeyName.EqualsIgnoreCase("VK_F6"))
        ret = PR_TRUE;
        break;
    case VK_F7:
      if(aKeyName.EqualsIgnoreCase("VK_F7"))
        ret = PR_TRUE;
        break;
    case VK_F8:
      if(aKeyName.EqualsIgnoreCase("VK_F8"))
        ret = PR_TRUE;
        break;
    case VK_F9:
      if(aKeyName.EqualsIgnoreCase("VK_F9"))
        ret = PR_TRUE;
        break;
    case VK_F10:
      if(aKeyName.EqualsIgnoreCase("VK_F10"))
        ret = PR_TRUE;
        break;
    case VK_F11:
      if(aKeyName.EqualsIgnoreCase("VK_F11"))
        ret = PR_TRUE;
        break;
    case VK_F12:
      if(aKeyName.EqualsIgnoreCase("VK_F12"))
        ret = PR_TRUE;
        break;
    case VK_F13:
      if(aKeyName.EqualsIgnoreCase("VK_F13"))
        ret = PR_TRUE;
        break;
    case VK_F14:
      if(aKeyName.EqualsIgnoreCase("VK_F14"))
        ret = PR_TRUE;
        break;
    case VK_F15:
      if(aKeyName.EqualsIgnoreCase("VK_F15"))
        ret = PR_TRUE;
        break;
    case VK_F16:
      if(aKeyName.EqualsIgnoreCase("VK_F16"))
        ret = PR_TRUE;
        break;
    case VK_F17:
      if(aKeyName.EqualsIgnoreCase("VK_F17"))
        ret = PR_TRUE;
        break;
    case VK_F18:
      if(aKeyName.EqualsIgnoreCase("VK_F18"))
        ret = PR_TRUE;
        break;
    case VK_F19:
      if(aKeyName.EqualsIgnoreCase("VK_F19"))
        ret = PR_TRUE;
        break;
    case VK_F20:
      if(aKeyName.EqualsIgnoreCase("VK_F20"))
        ret = PR_TRUE;
        break;
    case VK_F21:
      if(aKeyName.EqualsIgnoreCase("VK_F21"))
        ret = PR_TRUE;
        break;
    case VK_F22:
      if(aKeyName.EqualsIgnoreCase("VK_F22"))
        ret = PR_TRUE;
        break;
    case VK_F23:
      if(aKeyName.EqualsIgnoreCase("VK_F23"))
        ret = PR_TRUE;
        break;
    case VK_F24:
      if(aKeyName.EqualsIgnoreCase("VK_F24"))
        ret = PR_TRUE;
        break;
    case VK_NUM_LOCK:
      if(aKeyName.EqualsIgnoreCase("VK_NUM_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_SCROLL_LOCK:
      if(aKeyName.EqualsIgnoreCase("VK_SCROLL_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_COMMA:
      if(aKeyName.EqualsIgnoreCase("VK_COMMA"))
        ret = PR_TRUE;
        break;
    case VK_PERIOD:
      if(aKeyName.EqualsIgnoreCase("VK_PERIOD"))
        ret = PR_TRUE;
        break;
    case VK_SLASH:
      if(aKeyName.EqualsIgnoreCase("VK_SLASH"))
        ret = PR_TRUE;
        break;
    case VK_BACK_QUOTE:
      if(aKeyName.EqualsIgnoreCase("VK_BACK_QUOTE"))
        ret = PR_TRUE;
        break;
    case VK_OPEN_BRACKET:
      if(aKeyName.EqualsIgnoreCase("VK_OPEN_BRACKET"))
        ret = PR_TRUE;
        break;
    case VK_BACK_SLASH:
      if(aKeyName.EqualsIgnoreCase("VK_BACK_SLASH"))
        ret = PR_TRUE;
        break;
    case VK_CLOSE_BRACKET:
      if(aKeyName.EqualsIgnoreCase("VK_CLOSE_BRACKET"))
        ret = PR_TRUE;
        break;
    case VK_QUOTE:
      if(aKeyName.EqualsIgnoreCase("VK_QUOTE"))
        ret = PR_TRUE;
        break;
  }

  return ret;
}

PRBool 
nsXBLEventHandler::IsMatchingCharCode(const nsString& aChar, const nsString& aKeyName)
{
  PRBool ret = PR_FALSE;

  if (aChar == aKeyName)
    ret = PR_TRUE;

  return ret;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLEventHandler(nsIContent* aBoundElement, nsIContent* aHandlerElement, 
                      nsXBLEventHandler** aResult)
{
  *aResult = new nsXBLEventHandler(aBoundElement, aHandlerElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
