/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

/**
 * This class was needed for logging into the CS&T calendar server. 
 * It manages a string in the following form:
 *
 *    /S=Steve/G=Mansour/Nd=10000/
 *
 * It breaks up the string into its property name and value pairs
 * and allows easy access to individual properties.
 * My assumption here is that there won't be too many properties.
 * The class is initialized to deal with 50. That should be more than
 * adequate. It can deal with more automatically if it must.
 *
 * This code probably exists somewhere else.
 *
 * sman
 *
 */
#ifndef _NS_X400_PARSER_H
#define _NS_X400_PARSER_H

#include "nscalutilexp.h"

class NS_CAL_UTIL nsX400Parser
{
private:
  JulianString**  mppKeys;      /* keys are kept here */
  JulianString**  mppVals;      /* corresponding values are kept here */
  JulianString    msValue;      /* the string value with all the keys and vals */
  PRInt32         miSize;       /* size of the Keys and Vals arrays */
  PRInt32         miLength;     /* current number of entries in the keys and vals arrays */
  
  /**
   * Breaks the internal string msValue up into its component parts.
   * Keys are stored in the mppKeys array, corresponding values in the
   * mppVals array.
   * @return       NS_OK on success.
   */
  nsresult        Parse();

  /**
   * Assembles a new internal value string based on the contents of
   * the keys and vals arrays.
   * @return       NS_OK on success.
   */
  nsresult        Assemble();

  /**
   * create the keys and values arrays.
   * @return       NS_OK on success
   */
  nsresult        Init();

  /**
   * Search for the supplied key. Return its index if found.
   * @param aKey   the key to search for
   * @param aIndex the returned index where the key was found or -1
   *               to indicate that the key was not found.
   * @return       NS_OK on success.
   */
  nsresult        FindKey(const char* aKey, PRInt32* aIndex);

  /**
   * Make sure there's enough room to store a new property at
   * index aSize. Make more room if necessary.
   * @param aSize  the index where we want to add something
   * @return       NS_OK if there is enough room or if internal
   *               buffers were successfully grown to accommodate the
   *               requested index.
   */
  nsresult        EnsureSize(PRInt32 aSize);

  /**
   * Destroy the strings associated with the entry at the supplied index.
   * @param aIndex the index where we want to delete the entry
   * @return       NS_OK on success
   */
  nsresult        DestroyEntry(PRInt32 aIndex);

  /**
   * Destroy all key/value pairs
   * @return       NS_OK on success
   */
  nsresult        DestroyAllEntries();

  /**
   * @return       the current number of key/value pairs.
   */
  PRInt32         GetLength()   { return miLength; }

public:
                  nsX400Parser();
                  nsX400Parser(const char* psVal);
                  nsX400Parser(const JulianString& sVal);
  virtual         ~nsX400Parser();

  /**
   * Supply a new value for the parser. It should be of the form:
   * /S=Steve/G=Mansour/Nd=10000/
   * After this call, the values associated with keys can be accessed
   * or modified. New key/value pairs can be added. Existing key/value
   * pairs can be deleted.
   * @param aKey    the key
   * @return        NS_OK on success.
   */
  nsresult        SetValue(const char* psVal);

  /**
   * Get the newly assembled string
   * @param aStr    pointer to the character string. It refers
   *                back to the internal string maintained by this
   *                object. Do not modify this string.
   * @return        NS_OK on success
   */
  nsresult        GetValue(char** aStr);

  /**
   * Get the newly assembled string
   * @param aStr    reference to the string object.
   * @return        NS_OK on success
   */
  nsresult        GetValue(JulianString& aStr);

  /**
   * Set the supplied key to have the supplied value. Add the key
   * and value pair if they do not yet exist. Update the value 
   * if it already exists.
   * @param aKey    the key
   * @param aValue  the value to associate with the key
   * @return        NS_OK on success.
   */
  nsresult        Set(const char* aKey, const char* aVal);

  /**
   * Retrieve the value associated with the supplied key.
   * @param aKey    the key
   * @param aValue  the returned value associated with the key.
   *                *aValue will be 0 if the key was not found
   * @return        NS_OK on success.
   */
  nsresult        Get(const char* aKey, char **aVal );

  /**
   * Delete the key and value for the supplied key.
   * @param aKey    the key
   * @return        NS_OK on success.
   */
  nsresult        Delete(const char* psKey);

};

#endif  /* _NS_X400_PARSER_H */
