/**
 * nsCalSessionMgr.cpp: implementation of the nsCalSessionMgr class.
 * This class manages the list of calendars currently in memory.
 * All calendars that are to be displayed should be registered
 * in this list.
 */

#if !defined(AFX_NSCALSESSIONMGR_H__BC9A7773_3EF1_11D2_8ED1_000000000000__INCLUDED_)
#define AFX_NSCALSESSIONMGR_H__BC9A7773_3EF1_11D2_8ED1_000000000000__INCLUDED_

#include "jdefines.h"
#include "ptrarray.h"
#include "julnstr.h"
#include "nsCalSession.h"
#include "nscalexport.h"

class NS_CALENDAR nsCalSessionMgr  
{
  JulianPtrArray m_List;

public:

	nsCalSessionMgr();
	virtual ~nsCalSessionMgr();

  /**
   * Get a session to the supplied curl.
   * @param psCurl     the curl to the calendar store
   * @param psPassword the password needed for logging in
   * @param s          the session
   * @return 0 on success
   */
  nsresult GetSession(const char* psCurl, long lFlags, const char* psPassword, CAPISession& s);

  /**
   * Get a session to the supplied curl.
   * @param sCurl     the curl to the calendar store
   * @param psPassword the password needed for logging in
   * @param s          the session
   * @return 0 on success
   */
  nsresult GetSession(const JulianString sCurl, long lFlags, const char* psPassword, nsCalSession* &pCalSession);

  /**
   * Release a session
   * @param pUser pointer to the calendar to add
   * @return 0 on success
   *         1 if not found
   */
  nsresult ReleaseSession(CAPISession& s);

  /**
   * @return the session list
   */
  JulianPtrArray* GetList() {return &m_List;}

  /**
   * Shutdown and destroy all connected sessions.
   *
   * @return 0 on success
   *         otherwise the number of sessions that had problems shutting down.
   */
  nsresult Shutdown();

protected:
  /**
   * Search for an existing session to this curl.
   *
   * @param psCurl pointer to the curl of this calendar store
   * @param iStart start searching at this point in the list
   *               if iStart is < 0 it is snapped to 0. If it
   *               is >= list size, it is snapped to the last
   *               index.
   * @param piFound the index of the calendar of the list that
   *               matches the psCurl. This value is always
   *               >= iStart when the return value is 0. It 
   *               is returned as -1 if the curl cannot be found.
   * @return 0 on success
   *         1 if not found
   */
  nsresult Find(char* psCurl, int iStart, int* piFound);

  /**
   * Search for an existing session with the supplied CAPISession.
   *
   * @param s      the session to search for
   * @param iStart start searching at this point in the list
   *               if iStart is < 0 it is snapped to 0. If it
   *               is >= list size, it is snapped to the last
   *               index.
   * @param piFound the index of the calendar of the list that
   *               matches the psCurl. This value is always
   *               >= iStart when the return value is 0. It 
   *               is returned as -1 if the curl cannot be found.
   * @return 0 on success
   *         1 if not found
   */
  nsresult Find(CAPISession s, int iStart, int* piFound);

public:
  /**
   * Get the calendar session definition at the supplied index.
   * @param i the index to fetch
   * @return a pointer to the calendar session at the supplied index.
   */ 
  NS_IMETHOD_(nsCalSession*) nsCalSessionMgr::GetAt(int i);
};

#endif /* !defined(AFX_NSCALSESSIONMGR_H__BC9A7773_3EF1_11D2_8ED1_000000000000__INCLUDED_) */


