#ifndef _CMINICAL_EVENTS
#define _CMINICAL_EVENTS

class CMouseEvent
{
public:
    enum Type { MOUSEMOVE, MOUSEDOWN, MOUSEUP, MOUSECLICK, MOUSEDRAG };
    enum Modifiers { SHIFT, CTRL, ALT, META };

protected:
    int  m_iX;
    int  m_iY;
    int  m_iModifiers;     // a logical or of all applicable Modifiers
    Type m_eType;
    int  m_iButton;         // 0 = n/a, 1 = left, 2 = middle, 3 = right, ...

public:
    CMouseEvent( int iX, int iY, Type t, int iBtn, int iMod );
    virtual ~CMouseEvent();
    inline int  GetX()              { return m_iX; }
    inline int  GetY()              { return m_iY; }
    inline void SetButton(int i)    { m_iButton = i; }
    inline void SetModifiers(int i) { m_iModifiers = i; }
    inline int  GetButton()         { return m_iButton; }
    inline int  GetModifiers()      { return m_iModifiers; }
    inline Type GetType()           { return m_eType; }
    inline void SetShift()          { m_iModifiers |= (1 << (int)SHIFT); }
    inline int  GetShift()          { return m_iModifiers & (1 << (int)SHIFT); }
    inline void SetCtrl()           { m_iModifiers |= (1 << (int)CTRL); }
    inline int  GetCtrl()           { return m_iModifiers & (1 << (int)CTRL); }

};


#include <datetime.h>
class CMiniCalEvent : public CMouseEvent
{

public:
    enum Action { MNOTHING, MDATE, MDOW, MWEEK, MLEFTARROW, MRIGHTARROW };
    CMiniCalEvent(int iX, int iY, Type t, int iBtn, int iMod);
    virtual ~CMiniCalEvent();
    inline CMiniCalEvent::Action GetAction()            {return m_eAction;}
    inline void   SetAction(CMiniCalEvent::Action e)    {m_eAction = e;}
    inline int GetDOW()                                 {return m_iDOW;}
    inline void SetDOW(int i)                           {m_iDOW = i;}
    inline DateTime GetDate()                           {return m_DT;}
    inline void SetDate(DateTime d)                     {m_DT = d;}
    inline int GetWeekOffset()                          {return m_iWeekOffset;}
    inline void SetWeekOffset(int i)                    {m_iWeekOffset = i;}


protected:
    Action   m_eAction;
    int      m_iDOW;            // valid when eAction is MDOW
    int      m_iWeekOffset;     // valid when eAction is MWEEK
    DateTime m_DT;              // valid when eAction is DATE
};

#endif	/* _CMINICAL_EVENTS */
