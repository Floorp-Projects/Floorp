/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap.controls;

import java.io.*;
import netscape.ldap.client.JDAPBERTagDecoder;
import netscape.ldap.LDAPControl;
import netscape.ldap.ber.stream.*;

/**
 * Represents control data for returning paged results from a search.
 *
 * Example of usage, with JFC:
 *<PRE><CODE>
 *  // Call this to initialize the list box, whenever the search
 *  // conditions change.
 *  // "filter" may be "objectclass=person", for example
 *  void initListBox( String host, int port, String base, String filter ) {
 *
 *  // Create list box if not already done
 *   if ( _dataList == null ) {
 *       _dataList = new JList();
 *       JScrollPane scrollPane = new JScrollPane(_dataList);
 *       add( scrollPane );
 *   }
 *
 *   // Create a virtual data model
 *   vlistModel model = new vlistModel( host, port, base, filter );
 *   // Keep a buffer of one page before and one after
 *   model.setPageSize( getScrollVisibleSize() );
 *   _dataList.setModel( model );
 *  }
 *<P>
 * // Data model to supply buffer list data
 *class vlistModel extends AbstractListModel {
 *  vlistModel( String host, int port, String base, String filter ) {
 *      _base = base;
 *      _filter = filter;
 *
 *      // Connect to the server
 *      try {
 *          _ldc = new LDAPConnection();
 *          System.out.println( "Connecting to " + host +
 *                              ":" + port );
 *          _ldc.connect( host, port );
 *      } catch ( LDAPException e ) {
 *          System.out.println( e );
 *          _ldc = null;
 *      }
 *  }
 *
 *  // Called by JList to get virtual list size
 *    public int getSize() {
 *      if ( !_initialized ) {
 *          _initialized = true;
 *          _pageControls = new LDAPControl[2];
 *          // Paged results also require a sort control
 *          _pageControls[0] =
 *              new LDAPSortControl( new LDAPSortKey("cn"),
 *                                   true );
 *          // Do an initial search to get the virtual list size
 *          // Keep one page before and one page after the start
 *          _beforeCount = _pageSize;
 *          _afterCount = _pageSize;
 *          // Create the initial paged results control
 *          LDAPVirtualListControl cont =
 *              new LDAPVirtualListControl( "A",
 *                                          _beforeCount,
 *                                          _afterCount );
 *          _pageControls[1] = cont;
 *          _vlc = (LDAPVirtualListControl)_pageControls[1];
 *          getPage( 0 );
 *      }
 *      return _size;
 *  }
 *
 *  // Get a page starting at first (although we may also fetch
 *  // some preceding entries)
 *  boolean getPage( int first ) {
 *      _vlc.setRange( first, _beforeCount, _afterCount );
 *      return getPage();
 *  }
 *
 *  boolean getEntries() {
 *      // Specify necessary controls for vlv
 *      if ( _pageControls != null ) {
 *          try {
 *              _ldc.setOption( _ldc.SERVERCONTROLS, _pageControls );
 *          } catch ( LDAPException e ) {
 *              System.out.println( e + ", setting vlv control" );
 *          }
 *      }
 *      // Empty the buffer
 *      _entries.removeAllElements();
 *      // Do a search
 *      try {
 *          String[] attrs = { "cn" };
 *          LDAPSearchResults result =
 *              _ldc.search( _base,
 *                           LDAPConnection.SCOPE_SUB,
 *                           _filter,
 *                           attrs, false );
 *          while ( result.hasMoreElements() ) {
 *              LDAPEntry entry = (LDAPEntry)result.nextElement();
 *              LDAPAttribute attr = entry.getAttribute( attrs[0] );
 *              if ( attr != null ) {
 *                  Enumeration en = attr.getStringValues();
 *                  while( en.hasMoreElements() ) {
 *                      String name = (String)en.nextElement();
 *                      _entries.addElement( name );
 *                  }
 *              }
 *          }
 *      } catch ( LDAPException e ) {
 *          System.out.println( e + ", searching" );
 *          return false;
 *      }
 *      return true;
 *  }
 *
 *  // Fetch a buffer
 *  boolean getPage() {
 *      // Get the actual entries
 *      if ( !getEntries() )
 *          return false;
 *
 *      // Check if we have a control returned
 *      LDAPControl[] c = _ldc.getResponseControls();
 *      LDAPVirtualListResponse nextCont = null;
 *      for ( int i = 0; i < c.length; i++ ) {
 *          if ( c[i] instanceof LDAPVirtualListResponse ) {
 *              nextCont = (LDAPVirtualListResponse)c[i];
 *              break;
 *          }
 *      }
 *      if ( nextCont != null ) {
 *          _selectedIndex = nextCont.getFirstPosition() - 1;
 *          _top = Math.max( 0, _selectedIndex - _beforeCount );
 *          // Now we know the total size of the virtual list box
 *          _size = nextCont.getContentCount();
 *          _vlc.setListSize( _size );
 *      } else {
 *          System.out.println( "Null response control" );
 *      }
 *      return true;
 *  }
 *
 *  // Called by JList to fetch data to paint a single list item
 *    public Object getElementAt(int index) {
 *      if ( (index < _top) || (index >= _top + _entries.size()) ) {
 *          getPage( index );
 *      }
 *      int offset = index - _top;
 *      if ( (offset < 0) || (offset >= _entries.size()) )
 *          return new String( "No entry at " + index );
 *      else
 *          return _entries.elementAt( offset );
 *  }
 *
 *  // Called by application to find out the virutal selected index
 *    public int getSelectedIndex() {
 *      return _selectedIndex;
 *  }
 *
 *  // Called by application to find out the top of the buffer
 *    public int getFirstIndex() {
 *      return _top;
 *  }
 *
 *  public void setPageSize( int size ) {
 *      _pageSize = size;
 *  }
 *
 *  Vector _entries = new Vector();
 *  protected boolean _initialized = false;
 *  private int _top = 0;
 *  protected int _beforeCount;
 *  protected int _afterCount;
 *  private int _pageSize = 10;
 *  private int _selectedIndex = 0;
 *  protected LDAPControl[] _pageControls = null;
 *  protected LDAPVirtualListControl _vlc = null;
 *  protected int _size = -1;
 *  private String _base;
 *  private String _filter;
 *  private LDAPConnection _ldc;
 *}
 *</CODE></PRE>
 * <PRE>
 *   VirtualListViewRequest ::= SEQUENCE {
 *            beforeCount    INTEGER (0 .. maxInt),
 *            afterCount     INTEGER (0 .. maxInt),
 *            CHOICE {
 *                byIndex [0] SEQUENCE {
 *                    index           INTEGER,
 *                    contentCount    INTEGER
 *                }
 *                byFilter [1] jumpTo    Substring
 *            },
 *            contextID     OCTET STRING OPTIONAL
 *  }
 * </PRE>
 *
 * @version 1.0
 */

public class LDAPVirtualListControl extends LDAPControl {
    public final static String VIRTUALLIST = "2.16.840.1.113730.3.4.9";

    /**
     * Blank constructor for internal use in <CODE>LDAPVirtualListControl</CODE>.
     * @see netscape.ldap.LDAPControl
     */
    LDAPVirtualListControl() {
        super( VIRTUALLIST, true, null );
    }

    /**
     * Constructs a new <CODE>LDAPVirtualListControl</CODE> object. Use this
     * constructor on an initial search operation, specifying the first
     * entry to be matched, or the initial part of it.
     * @param jumpTo An LDAP search expression defining the result set.
     * @param beforeCount The number of results before the top/center to
     * return per page.
     * @param afterCount The number of results after the top/center to
     * return per page.
     * @see netscape.ldap.LDAPControl
     */
    public LDAPVirtualListControl( String jumpTo, int beforeCount,
                                   int afterCount  ) {
        super( VIRTUALLIST, true, null );
        setRange( jumpTo, beforeCount, afterCount );
    }

    public LDAPVirtualListControl( String jumpTo, int beforeCount,
                                   int afterCount, String context  ) {
        this( jumpTo, beforeCount, afterCount );
        m_context = context;
    }

    /**
     * Constructs a new <CODE>LDAPVirtualListControl</CODE> object. Use this
     * constructor on a subsquent search operation, after we know the
     * size of the virtual list, to fetch a subset.
     * @param startIndex The index into the virtual list of an entry to
     * return.
     * @param beforeCount The number of results before the top/center to
     * return per page.
     * @param afterCount The number of results after the top/center to
     * return per page.
     * @see netscape.ldap.LDAPControl
     */
    public LDAPVirtualListControl( int startIndex, int beforeCount,
                                   int afterCount, int contentCount  ) {
        super( VIRTUALLIST, true, null );
        m_listSize = contentCount;
        setRange( startIndex, beforeCount, afterCount );
    }

    public LDAPVirtualListControl( int startIndex, int beforeCount,
                                   int afterCount, int contentCount,
                                   String context ) {
        this( startIndex, beforeCount, afterCount, contentCount );
        m_context = context;
    }

    /**
     * Sets the starting index, and the number of entries before and after
     * to return. Apply this method to a control returned from a previous
     * search, to specify what result range to return on the next search.
     * @param startIndex The index into the virtual list of an entry to
     * return.
     * @param beforeCount The number of results before startIndex to
     * return per page.
     * @param afterCount The number of results after startIndex to
     * return per page.
     * @see netscape.ldap.LDAPControl
     */
    public void setRange( int startIndex, int beforeCount, int afterCount  ) {
        m_beforeCount = beforeCount;
        m_afterCount = afterCount;
        m_listIndex = startIndex;
        m_value = createPageSpecification( m_listIndex, m_listSize,
                                           m_beforeCount, m_afterCount );
    }

    /**
     * Sets the search expression, and the number of entries before and after
     * to return.
     * @param jumpTo An LDAP search expression defining the result set.
     * return.
     * @param beforeCount The number of results before startIndex to
     * return per page.
     * @param afterCount The number of results after startIndex to
     * return per page.
     * @see netscape.ldap.LDAPControl
     */
    public void setRange( String jumpTo, int beforeCount, int afterCount  ) {
        m_beforeCount = beforeCount;
        m_afterCount = afterCount;
        m_value = createPageSpecification( jumpTo, m_beforeCount, m_afterCount );
    }

    /**
     * Gets the size of the virtual result set.
     * @return The size of the virtual result set, or -1 if not known.
     */
    public int getIndex() {
        return m_listIndex;
    }

    /**
     * Gets the size of the virtual result set.
     * @return The size of the virtual result set, or -1 if not known.
     */
    public int getListSize() {
        return m_listSize;
    }

    /**
     * Sets the size of the virtual result set.
     * @param listSize The virtual result set size.
     */
    public void setListSize( int listSize ) {
        m_listSize = listSize;
    }

    /**
     * Gets the number of results before the top/center to return per page.
     * @return The number of results before the top/center to return per page.
     */
    public int getBeforeCount() {
        return m_beforeCount;
    }

    /**
     * Gets the number of results after the top/center to return per page.
     * @return The number of results after the top/center to return per page.
     */
    public int getAfterCount() {
        return m_afterCount;
    }

    /**
     * Gets the optional context cookie
     * @return The optional context cookie
     */
    public String getContext() {
        return m_context;
    }

    /**
     * Sets the optional context cookie
     * @param context The optional context cookie
     */
    public void setContext( String context ) {
        m_context = context;
    }

    /**
     * Creates a "flattened" BER encoding of the requested page
     * specifications and return it as a byte array.
     * @param subFilter Filter expression for generating the results.
     * @param beforeCount Number of entries before first match to return.
     * @param afterCount Number of entries ager first match to return.
     * @return The byte array of encoded data.
     */
    private byte[] createPageSpecification( String subFilter,
                                            int beforeCount,
                                            int afterCount ) {
        /* A sequence */
        BERSequence seq = new BERSequence();
        seq.addElement( new BERInteger( beforeCount ) );
        seq.addElement( new BERInteger( afterCount ) );
        seq.addElement( new BERTag(BERTag.CONTEXT|TAG_BYFILTER,
                                   new BEROctetString(subFilter), true) );
        if ( m_context != null ) {
            seq.addElement( new BEROctetString(m_context) );
        }
        /* Suck out the data and return it */
        return flattenBER( seq );
    }

    /**
     * Creates a "flattened" BER encoding of the requested page
     * specifications and return it as a byte array.
     * @param listIndex The center or starting entry to return.
     * @param listSize The virtual list size.
     * @param beforeCount Number of entries before first match to return.
     * @param afterCount Number of entries after first match to return.
     * @return The byte array of encoded data.
     */
    private byte[] createPageSpecification( int listIndex,
                                            int listSize,
                                            int beforeCount,
                                            int afterCount ) {
        /* A sequence */
        BERSequence seq = new BERSequence();
        seq.addElement( new BERInteger( beforeCount ) );
        seq.addElement( new BERInteger( afterCount ) );
        /* A sequence of list index and list size */
        BERSequence indexSeq = new BERSequence();
        indexSeq.addElement( new BERInteger(listIndex) );
        indexSeq.addElement( new BERInteger(listSize) );
        seq.addElement(
            new BERTag(BERTag.CONTEXT|BERTag.CONSTRUCTED|TAG_BYINDEX,
                       indexSeq,true) );
        if ( m_context != null ) {
            seq.addElement( new BEROctetString(m_context) );
        }

        /* Suck out the data and return it */
        return flattenBER( seq );
    }

    public String toString() {
         StringBuffer sb = new StringBuffer("{VirtListCtrl:");
        
        sb.append(" isCritical=");
        sb.append(isCritical());
        
        sb.append(" beforeCount=");
        sb.append(m_beforeCount);
        
        sb.append(" afterCount=");
        sb.append(m_afterCount);
        
        sb.append(" listIndex=");
        sb.append(m_listIndex);

        sb.append(" listSize=");
        sb.append(m_listSize);

        if (m_context != null) {
            sb.append(" conext=");
            sb.append(m_context);
        }

        sb.append("}");

        return sb.toString();
    }
    
    private final static int TAG_BYINDEX = 0;
    private final static int TAG_BYFILTER = 1;
    private int m_beforeCount = 0;
    private int m_afterCount = 0;
    private int m_listIndex = -1;
    private int m_listSize = 0;
    private String m_context = null;
}
