/**
 * nsCalList.cpp: implementation of the nsCalList class.
 * This class manages the list of calendars currently in memory.
 * All calendars that are to be displayed should be registered
 * in this list.
 */

#if !defined(AFX_NSCALLIST_H__BC9A7773_3EF1_11D2_8ED1_000000000000__INCLUDED_)
#define AFX_NSCALLIST_H__BC9A7773_3EF1_11D2_8ED1_000000000000__INCLUDED_

#include "jdefines.h"
#include "ptrarray.h"
#include "nscalexport.h"

class NS_CALENDAR nsCalList  
{
public:
  JulianPtrArray m_List;

	nsCalList();
	virtual ~nsCalList();

  /**
   * Add a calendar to the list
   * @param pCal pointer to the calendar to add
   * @return 0 on success
   */
  nsresult Add(NSCalendar* pCal);

  /**
   * Delete the calendar matching the supplied pointer.
   * @param pCal pointer to the calendar to add
   * @return 0 on success
   *         1 if not found
   */
  nsresult Delete(NSCalendar* pCal);

  /**
   * Delete all calendars having the supplied cal url
   * @param pCurl pointer to the curl of this calendar store
   * @return 0 on success
   *         1 if not found
   */
  nsresult Delete(char* psCurl);

  /**
   * Search for a calendar.
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
   * Get the calendar at the supplied index.
   * @param i the index of the calendar to fetch
   * @return 0 if the index is bad, otherwise it is a pointer
   *           to the calendar at index i in the list
   */ 
  NSCalendar* GetAt(int i);

};

#endif /* !defined(AFX_NSCALLIST_H__BC9A7773_3EF1_11D2_8ED1_000000000000__INCLUDED_) */


