/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * A class for keeping an ordered list of Strings
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "String.h"
#include "baseutils.h"

#ifndef MITREXSL_STRINGLIST_H
#define MITREXSL_STRINGLIST_H

class StringList {
   friend class StringListIterator;

   public:

      /**
       * Creates an empty StringList
      **/
      StringList();

      /**
       * StringList destructor
      **/
      ~StringList();

       MBool contains(String& search);

      /**
       * Returns the number of Strings in this List
      **/
      Int32 getLength();

      /**
       * Returns a StringListIterator for this StringList
      **/
      StringListIterator& iterator();

      /**
       * Adds the given String to the list
      **/
      void add(String* strptr);

      /**
       * Removes the given String pointer from the list
      **/
      String* remove(String* strptr);

      /**
       * Removes all Strings equal to the given String from the list
       * All removed strings will be destroyed
      **/
      void remove(String& search);

protected:
    struct StringListItem {
        StringListItem* nextItem;
        StringListItem* prevItem;
        String* strptr;
    };

private:

      StringListItem* firstItem;
      StringListItem* lastItem;
      Int32 itemCount;

      void insertAfter(String* strptr, StringListItem* sItem);
      void insertBefore(String* strptr, StringListItem* sItem);

      /**
       * Removes the given StringListItem pointer from the list
      **/
      StringListItem* remove(StringListItem* sItem);
};




class StringListIterator {

public:


   StringListIterator(StringList* list);
   ~StringListIterator();

   void    add(String* strptr);

   MBool  hasNext();

   MBool  hasPrevious();

   String* next();

   String* previous();

   String* remove();

   void    reset();

private:

   StringList::StringListItem* currentItem;

   StringList* stringList;
   MBool allowRemove;
};

#endif
