/*-*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *  $Id: QtHistoryContext.cpp,v 1.1 1998/09/25 18:01:29 ramiro%netscape.com Exp $
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * First version of this file written 1998 by Jonas Utterstrom 
 * <mailto:jonas.utterstrom@vittran.norrnod.se>
 */


#include "QtHistoryContext.h"
#include "DialogPool.h"

#include <qmainwindow.h>
#include <qmsgbox.h>
#include <qtimer.h>
#include <qscrollview.h>
#include <qapp.h>
#include <qlistview.h>
#include <qdir.h>
#include <qaccel.h>
#include <qmenubar.h> 


#include "prefapi.h"
#include "layers.h"
#include "icons.h"
#include "menus.h"
#include "QtBrowserContext.h"
#include "histlistview.h"
#include <xplocale.h>

#define i18n(x) x

static const int TITLE                   = 0;
static const int LOCATION                = 1;
static const int LASTVISITED             = 2;
static const int FIRSTVISITED            = 3;
static const int EXPIRES                 = 4;
static const int VISITCOUNT              = 5;


//
// HistoryItem
//

class HistoryViewItem  : public QListViewItem
{
    public:
        HistoryViewItem(QListView* parent);
        virtual const char* key(int, bool) const;
};

HistoryViewItem::HistoryViewItem(QListView* parent)
    : QListViewItem(parent)
{
}

const char* HistoryViewItem::key(int column, bool ascending) const
{
    char *exitc;
    const char* currc;
    static char buffer[1024]; // warning, overflow possible
    int   count;
    const char* ret = text(column);

    if (ret==0)
        return 0; // hmm, no string in column, maybe is something wrong

    switch (column)
    {
        case TITLE:
            // Convert title to downcase letters to get correct sorting
            currc = ret;
            ret = exitc = buffer;
            while ((*currc)!='\0')
                *exitc++ = tolower(*currc++);
            break;
        case VISITCOUNT:
            // Insert padding zeros to get correct sorting in the visit count column
            count = strtol(ret, &exitc, 10);
            if ((*exitc)=='\0')
            {
                sprintf(buffer, "%09d\n", count);
                ret = buffer;
            }
            break;
        default:
            break;
    }
    return ret;
}

//
// QtHistoryContext
//

static QtHistoryContext *gpHistory = 0;

static void cleanupHistory()
{
    delete gpHistory;
    gpHistory = 0;
}


QtHistoryContext* QtHistoryContext::ptr()
{
    if ( !gpHistory ) 
    {	
        MWContext* context = XP_NewContext();
        context->type = MWContextHistory;
        gpHistory = new QtHistoryContext(context);
        qAddPostRoutine(cleanupHistory);
    }
    return gpHistory;
}



QtHistoryContext::QtHistoryContext(MWContext *cx)
    : QtContext(cx)
{
    historyWidget     = 0;
    listView          = 0;
    sortColumn	      = LASTVISITED;
    sortAscending     = TRUE;
    m_histCursor      = 0;
}

QtHistoryContext::~QtHistoryContext()
{
    GH_ReleaseContext(m_histCursor, TRUE);
}

QtHistoryContext* QtHistoryContext::qt( MWContext* c )
{
    return 0;
}

void QtHistoryContext::createWidget()
{
    if ( historyWidget )
	return;
    historyWidget = new QMainWindow( 0, "Main History widget" );
    // use default geometry, etc. as if we are the main widget
    qApp->setMainWidget(historyWidget);
    // but there isn't one - multiple browsers all equal.
    qApp->setMainWidget(0);
    populateMenuBar(historyWidget->menuBar());

    listView = new HistoryListView( historyWidget, "History listview");

    listView->setAllColumnsShowFocus(TRUE);
    listView->setSorting( -1  );
    listView->setCaption( i18n("History") );
    listView->addColumn( i18n("Name"), 150 );
    listView->addColumn( i18n("Location"), 150 );
    listView->addColumn( i18n("Last Visited"), 100 );
    listView->addColumn( i18n("First Visited"), 100 );
    listView->addColumn( i18n("Expires"), 100 );
    listView->addColumn( i18n("Visit Count"), 100 );
    historyWidget->resize( 600, 600 );

    // connections
    connect(listView,  SIGNAL(doubleClicked(QListViewItem*)),
            SLOT(activateItem(QListViewItem*)) );

    connect(listView, SIGNAL(returnPressed(QListViewItem*)),
            SLOT(activateItem(QListViewItem*)) );

    connect(listView, SIGNAL(selectionChanged(QListViewItem*)),
            SLOT(newSelected(QListViewItem*)) );

    connect(listView, 
            SIGNAL(rightButtonPressed(QListViewItem*,const QPoint&,int)),
            SLOT(rightClick(QListViewItem*,const QPoint&,int)) );
    
    connect(listView,
            SIGNAL(sortColumnChanged(int, bool)),
            SLOT(sortColumnChanged(int, bool)));


    historyWidget->setCentralWidget( listView );
}

void QtHistoryContext::sortColumnChanged(int column, bool ascending)
{
    QMenuBar *menuBar = historyWidget->menuBar();

    menuBar->setItemChecked(MENU_HIST_VIEW_SORTBYTITLE,            column==TITLE);
    menuBar->setItemChecked(MENU_HIST_VIEW_SORTBYLOCATION,         column==LOCATION);
    menuBar->setItemChecked(MENU_HIST_VIEW_SORTBYDATEFIRSTVISITED, column==FIRSTVISITED);
    menuBar->setItemChecked(MENU_HIST_VIEW_SORTBYDATELASTVISITED,  column==LASTVISITED);
    menuBar->setItemChecked(MENU_HIST_VIEW_EXPIRATIONDATE,         column==EXPIRES);
    menuBar->setItemChecked(MENU_HIST_VIEW_VISITCOUNT,             column==VISITCOUNT);

    menuBar->setItemChecked(MENU_HIST_VIEW_SORTASCENDING, ascending);
    menuBar->setItemChecked(MENU_HIST_VIEW_SORTDESCENDING, !ascending);

    sortColumn    = column;
    sortAscending = ascending;
}

void QtHistoryContext::invalidateListView()
{
    if (listView) 
        listView->clear();
}


#define BUF_SIZ 1024

void QtHistoryContext::cmdOpenBrowser()
{
    int bufsiz = BUF_SIZ;
    char buf[ BUF_SIZ ];
    URL_Struct *url;

    if( PREF_GetCharPref( "browser.startup.homepage", buf, &bufsiz ) < 0 || !strlen(buf) )
    {
        clearView(0);
        alert(tr("No home document specified."));
        return;
    }
    url = NET_CreateURLStruct (buf, NET_DONT_RELOAD); 
    QtContext::makeNewWindow( url,0,0,0);
}


void QtHistoryContext::cmdComposeMessage()
{
}

void QtHistoryContext::cmdNewBlank()
{
}

void QtHistoryContext::cmdNewTemplate()
{
}

void QtHistoryContext::cmdNewWizard()
{
}

void QtHistoryContext::cmdAddBookmark()
{
}

void QtHistoryContext::cmdSaveAs()
{
    GH_FileSaveAsHTML(m_histCursor, mwContext());
}

void QtHistoryContext::cmdGotoPage()
{
    activateItem(listView->currentItem());
}


void QtHistoryContext::cmdOpenSelected()
{
}

void QtHistoryContext::cmdOpenAddToToolbar()
{
}

void QtHistoryContext::cmdClose()
{
    close(TRUE);
}

void QtHistoryContext::cmdExit()
{
}


void QtHistoryContext::cmdUndo()
{
}

void QtHistoryContext::cmdRedo()
{
}

void QtHistoryContext::cmdCut()
{
}

void QtHistoryContext::cmdCopy()
{
}

void QtHistoryContext::cmdPaste()
{
}


void QtHistoryContext::cmdDelete()
{
    createWidget();
}


void QtHistoryContext::changeSorting( int column, bool ascending)
{
    listView->setSorting(column, ascending);
}

void QtHistoryContext::cmdSortByTitle()
{    
    changeSorting(TITLE, sortAscending );
}

void QtHistoryContext::cmdSortByLocation()
{
    changeSorting(LOCATION, sortAscending);
}

void QtHistoryContext::cmdSortByDateLastVisited()
{
    changeSorting(LASTVISITED, sortAscending);
}

void QtHistoryContext::cmdSortByDateFirstVisited()
{
    changeSorting(FIRSTVISITED, sortAscending);
}

void QtHistoryContext::cmdSortByExpirationDate()
{
    changeSorting(EXPIRES, sortAscending);
}

void QtHistoryContext::cmdSortByVisitCount()
{
    changeSorting(VISITCOUNT, sortAscending);
}


void QtHistoryContext::cmdSortAscending()
{
    changeSorting(sortColumn, true);
}

void QtHistoryContext::cmdSortDescending()
{
    changeSorting(sortColumn, false);
}


void QtHistoryContext::cmdAboutMozilla()
{
}

void QtHistoryContext::cmdAboutQt()
{
    QMessageBox::aboutQt( 0 );
}

void QtHistoryContext::cmdOpenHistory()
{
}

void QtHistoryContext::cmdAddToToolbar()
{
}


void QtHistoryContext::cmdOpenNavCenter()
{
}

void QtHistoryContext::cmdOpenOrBringUpBrowser()
{
}

void QtHistoryContext::cmdOpenEditor()
{
}



void QtHistoryContext::gotoEntry(const char* url, const char* target)
{
    QtContext::makeNewWindow( NET_CreateURLStruct(url,NET_DONT_RELOAD),0,0,0);
}



void QtHistoryContext::activateItem( QListViewItem *i )
{
    const char *url = i->text(LOCATION);
    QtContext::makeNewWindow( NET_CreateURLStruct(url, NET_DONT_RELOAD), 0, 0, 0);
}

void QtHistoryContext::rightClick(QListViewItem *i, const QPoint &p, int)
{
}

void QtHistoryContext::newSelected( QListViewItem *i )
{
}

void QtHistoryContext::show()
{
    createWidget();
    refreshHistory();
    changeSorting(sortColumn, sortAscending);
    historyWidget->show();
    historyWidget->raise();
}

gh_SortColumn QtHistoryContext::togh_SortColumn(int column) const
{
    gh_SortColumn ret;
    switch (column)
    {
        case TITLE:
            ret = eGH_NameSort;
            break;
        case LOCATION:
            ret = eGH_LocationSort;
            break;
        case FIRSTVISITED:
            ret = eGH_FirstDateSort;
            break;
        case LASTVISITED:
        case EXPIRES:
            ret =  eGH_LastDateSort;
            break;
        case VISITCOUNT:
            ret = eGH_VisitCountSort;
            break;
        default:
            ret = eGH_NoSort;
            break;
    }
    return ret;
}

void  QtHistoryContext::refreshHistory()
{
    int totalLines;
    gh_SortColumn sortCol = togh_SortColumn(sortColumn);

    if (m_histCursor!=0)
        GH_ReleaseContext(m_histCursor, TRUE);
    m_histCursor = GH_GetContext(sortCol, 0, 
                                 &QtHistoryContext::notifyHistory,
                                 0, this );

    XP_ASSERT(m_histCursor);
    totalLines = GH_GetNumRecords(m_histCursor);
    refreshCells(0, totalLines);
}

int QtHistoryContext::notifyHistory(gh_NotifyMsg *msg)
{
    QtHistoryContext *obj = (QtHistoryContext*)msg->pUserData;
    obj->refreshHistory();
    return TRUE;
}

void QtHistoryContext::setHistoryRow(QListViewItem *item, gh_HistEntry *gh_entry)
{
    setColumnText(item, TITLE, gh_entry);
    setColumnText(item, LOCATION, gh_entry);
    setColumnText(item, LASTVISITED, gh_entry);
    setColumnText(item, FIRSTVISITED, gh_entry);
    setColumnText(item, EXPIRES, gh_entry);
    setColumnText(item, VISITCOUNT, gh_entry);
}


void QtHistoryContext::refreshCells(int first, int last)
{
    int ix;
    gh_HistEntry* gh_entry;
    QListViewItem *current = listView->firstChild();

    // Find first index
    for (ix = 0; ix<first; ix++)
    {
        if (current==0) 
            return; // something is fishy
        current = current->nextSibling();
    }

    // Update history values
    for (ix = first; ix<last; ix++)
    {
        gh_entry = GH_GetRecord(m_histCursor, ix);
        if (current==0)  break;
        if (gh_entry!=0) 
        {
            setHistoryRow(current, gh_entry);
            current = current->nextSibling();
        }
    }

    // Insert new entries if there wasn't enough rows
    for (; ix<last; ix++)
    {
        gh_entry = GH_GetRecord(m_histCursor, ix);
        if (gh_entry!=0)
        {
            current = new HistoryViewItem(listView);
            setHistoryRow(current, gh_entry);
        }
    }
    
}


void QtHistoryContext::setColumnText(QListViewItem *item, int col, gh_HistEntry *e)
{
    static char tmp_buf[1024];
    const char *coltext = tmp_buf;
    struct tm* timetm;
    if (e==0 || item==0) return; // missing arguments, return
    switch (col)
    {
        case TITLE:
            coltext = e->pszName;
            break;
        case LOCATION:
            coltext = e->address;
            break;
        case FIRSTVISITED:
            if (e->first_accessed==0) break; // no access, skip 
            timetm = localtime(&(e->first_accessed));
            FE_StrfTime(mwContext(), tmp_buf, 1024,
                        XP_DATE_TIME_FORMAT, timetm);
            break;
        case LASTVISITED:
            if (e->last_accessed==0) break; // no last access, skip
            timetm = localtime(&(e->last_accessed));
            FE_StrfTime(mwContext(), tmp_buf, 1024,
                        XP_DATE_TIME_FORMAT, timetm);
            break;
        case EXPIRES:
        {
            int32  expireDays;
            time_t expireSecs;
            
            if (e->last_accessed==0) break; // no last access, skip
            PREF_GetIntPref("browser.link_expiration",&expireDays);
            expireSecs  = e->last_accessed;
            expireSecs += expireDays*60*60*24;
            timetm = localtime(&expireSecs);
            FE_StrfTime(mwContext(), tmp_buf, 1024,
                        XP_DATE_TIME_FORMAT, timetm);
            break;
        }
        case VISITCOUNT:
            if (e->iCount==0) break; // no count, skip
            sprintf(tmp_buf, "%d", e->iCount);
            break;
        default:
            // something is fishy
            XP_ASSERT(0);
    };
    item->setText(col, coltext);
}
