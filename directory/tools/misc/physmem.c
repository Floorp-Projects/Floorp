/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1996-2003
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 *	Mark Smith <MarkCSmithWork@aol.com>
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

#include <unistd.h>
#include <stdio.h>

int
main( int argc, char **argv )
{
    long	pagesize, phys_pages;

    if (( pagesize = sysconf( _SC_PAGESIZE )) == -1 ) {
	perror( "sysconf( _SC_PAGESIZE )" );
	return( 1 );
    }

    if (( phys_pages = sysconf( _SC_PHYS_PAGES )) == -1 ) {
	perror( "sysconf( _SC_PHYS_PAGES )" );
	return( 1 );
    }

    /* XXX pagesize * phys_pages may not fit in a long! */
    printf( "pagesize: %ld, pages: %ld, memory: %ldK\n",
	    pagesize, phys_pages, pagesize * phys_pages / 1024L );

    return( 0 );
}
