/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is XSL:P XSLT processor.
 *
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999-2000 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: StringComparator.h,v 1.1 2000/04/12 10:49:39 kvisco%ziplink.net Exp $
 */

 #include "String.h"
 #include "TxObject.h"

 #ifndef TRANSFRMX_STRING_COMPARATOR_H
 #define TRANSFRMX_STRING_COMPARATOR_H

/*
   An interface for handling String comparisons
*/
class StringComparator : public TxObject {

public:

    static const String EN_LANG;

    /**
     * Returns an instance of the StringComparator which handles the given Language
     * <BR>
     * Note: Remember to destroy instance when done.
    **/
    static StringComparator* getInstance(const String& lang);

    /**
     * Compares the given Strings. -1 is returned if str1 is less than str2,
     * 0 is returned if the two Strings are equal, and 1  is return if str1 is
     * greater than str2.
    **/
    virtual int compare(const String& str1, const String& str2) = 0;

};

/**
 * The default StringComparator, a very simple implementation.
**/
class DefaultStringComparator : public StringComparator {

public:

    /**
     * Creates a new DefaultStringComparator
    **/
    DefaultStringComparator();

    /**
     * Destroys this StringComparator
    **/
    virtual ~DefaultStringComparator();

    /**
     * Compares the given Strings. -1 is returned if str1 is less than str2,
     * 0 is returned if the two Strings are equal, and 1  is return if str1 is
     * greater than str2.
    **/
    virtual int compare(const String& str1, const String& str2);


};
#endif

