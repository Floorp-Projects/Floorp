#include "jdefines.h"
#include "events.h"

/*
 * TODO: Get rid of this class! Use the standars nsGUIEvent for GUI events
 * and create a separate event class for calendaring and controllers
 */

CMouseEvent::CMouseEvent( int iX, int iY, CMouseEvent::Type t, int iBtn, int iMod )
{
    m_iX = iX;
    m_iY = iY;
    m_iModifiers = iMod;
    m_iButton = iBtn;
    m_eType = t;
}

CMouseEvent::~CMouseEvent()
{
}

CMiniCalEvent::CMiniCalEvent( int iX, int iY, CMouseEvent::Type t, int iBtn, int iMod ) : CMouseEvent( iX,iY,t, iBtn, iMod )
{
    m_eAction = MNOTHING;
    m_iDOW = 0;
    m_iWeekOffset = 0;
}

CMiniCalEvent::~CMiniCalEvent()
{
}
