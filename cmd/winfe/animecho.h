#ifndef __ANIMECHO_H
//	Avoid include redundancy
//
#define __ANIMECHO_H

class CDDEAnimationEcho
{
protected:
	static CPtrList m_Registry;
	POSITION m_rIndex;
	CString m_csServiceName;
	
	
	CDDEAnimationEcho(CString& csServiceName) 
	{
		m_rIndex = m_Registry.AddTail(this);
		m_csServiceName = csServiceName;
	}

	~CDDEAnimationEcho()	
	{
		m_Registry.RemoveAt(m_rIndex);
	}
	
	//	Must override.
	void EchoAnimation(DWORD dwWindowID, DWORD dwAnimationState);
	
	
public:
	CString GetServiceName()	{
		return(m_csServiceName);
	}

	//	Consider these the constructor, destructor.
	static void DDERegister(CString &csServiceName);
	static BOOL DDEUnRegister(CString &csServiceName);
	
	static void Echo(DWORD dwWindowID, DWORD dwAnimationState);

};

#endif // __URLECHO_H
