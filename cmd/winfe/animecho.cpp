#include "stdafx.h"

#include "animecho.h"

#include "wfedde.h"

//  Static property.
CPtrList CDDEAnimationEcho::m_Registry;

void CDDEAnimationEcho::Echo(DWORD dwWindowID, DWORD dwState)  
{
  //  Go through the registry.
  CDDEAnimationEcho *pItem;
  POSITION rIndex = m_Registry.GetHeadPosition();
  while(rIndex != NULL) 
  {
    pItem = (CDDEAnimationEcho*)m_Registry.GetNext(rIndex);
    
    pItem->EchoAnimation(dwWindowID, dwState);
  }
}

void CDDEAnimationEcho::DDERegister(CString &csServiceName)  
{
  //  Just create the item.  Our job's done.
  CDDEAnimationEcho *pDontCare = new CDDEAnimationEcho(csServiceName);
}

BOOL CDDEAnimationEcho::DDEUnRegister(CString &csServiceName)  
{
  //  Go through all registered items.
  POSITION rIndex = m_Registry.GetHeadPosition();
  CDDEAnimationEcho *pDelMe;
  
  while (rIndex != NULL) 
  {
    pDelMe = (CDDEAnimationEcho*)m_Registry.GetNext(rIndex);
    
    if(pDelMe->GetServiceName() == csServiceName)  
	{
       delete pDelMe;
       return TRUE;
    }
  }

  return FALSE;
}

void CDDEAnimationEcho::EchoAnimation(DWORD dwWindowID, DWORD dwState)  
{
  CDDEWrapper::AnimationEcho(this, dwWindowID, dwState);
}
