
#include "global.h"

#include "GeckoContainer.h"


nsresult GeckoContainerUI::CreateBrowserWindow(PRUint32 aChromeFlags,
             GeckoContainer *aParent,
             GeckoContainer **aNewWindow)
{
    return NS_ERROR_FAILURE;
}

void GeckoContainerUI::Destroy()
{
}

void GeckoContainerUI::Destroyed()
{
}

void GeckoContainerUI::SetFocus()
{
}

void GeckoContainerUI::KillFocus()
{
}

void GeckoContainerUI::UpdateStatusBarText(const PRUnichar* aStatusText)
{
}

void GeckoContainerUI::UpdateCurrentURI()
{
}

void GeckoContainerUI::UpdateBusyState(PRBool aBusy)
{
}

void GeckoContainerUI::UpdateProgress(PRInt32 aCurrent, PRInt32 aMax)
{
}

void GeckoContainerUI::GetResourceStringById(PRInt32 aID, char ** aReturn)
{
}

void GeckoContainerUI::ShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
}

void GeckoContainerUI::ShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
}

void GeckoContainerUI::HideTooltip()
{
}

void GeckoContainerUI::ShowWindow(PRBool aShow)
{
}

void GeckoContainerUI::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
}

void GeckoContainerUI::EnableChromeWindow(PRBool aEnabled)
{
}

PRUint32 GeckoContainerUI::RunEventLoop(PRBool &aRunCondition)
{
    return 0;
}
