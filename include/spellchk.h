/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* *
 *  
 *
 ***************************************************************************
 *  spellchk.h
 *  Public header file for the Spell Checker library.
 ****************************************************************************/

#ifndef _SPELLCHK_H_
#define _SPELLCHK_H_

/* In WIN16 DLL exported functions require __loadds keywords for the DS
 * register to be set to the DLL's data-segment value and restored on return
 * from the function.
 */
#ifdef WINDOWS
#define SCAPI __loadds
#elif defined(WIN32)
#define SCAPI __cdecl
#else
#define SCAPI
#endif

#ifdef MAC
#include <Files.h>
#endif

/* Language and Dialect codes.  */

#define L_AFRIKAANS     101
#define L_CATALAN       102
#define L_CZECH         103
#define L_DANISH        104
#define L_DUTCH         105
#define L_ENGLISH       106
#define L_FINNISH       107
#define L_FRENCH        108
#define L_GERMAN        109
#define L_GREEK         110
#define L_HUNGARIAN     111
#define L_ITALIAN       112
#define L_NORWEGIAN     113
#define L_POLISH        114
#define L_PORTUGUESE    115
#define L_RUSSIAN       116
#define L_SPANISH       117
#define L_SWEDISH       118


#define D_DEFAULT       0xFFFF
#define D_AUS_ENGLISH   0x1001
#define D_US_ENGLISH    0x1010
#define D_UK_ENGLISH    0x1100
#define D_DOPPEL        0x2001     /* German */
#define D_SCHARFES      0x2010
#define D_BRAZILIAN     0x4001     /* Portuguese */
#define D_EUROPEAN      0x4010
#define D_BOKMAL        0x8001     /* Norwegian */
#define D_NYNORSK       0x8010


/* ISpellChecker - This class specifies the interface to the Spell Checker. A client
 * application instatiates a spell checker object using the exported function SC_Create().
 * The client application then performs spell checking using the member functions of this
 * class. The spell checker object is destroyed by using the exported function SC_Destroy().
 */

class ISpellChecker
{
public:
    /* Needs to be called by the client app once before calling any other functions.
     * Return:  0 = success, non-zero = error
     */ 
#ifdef MAC
    virtual int SCAPI Initialize(int LangCode, int DialectCode, 
                                 FSSpec *DbPath, FSSpec *PersonalDbFile) = 0;
#else
    virtual int SCAPI Initialize(int LangCode, int DialectCode, 
                                 const char *DbPath, const char *PersonalDbFile) = 0;
#endif

    /* Functions to set and get the current language and dialect settings.
     * Returns: 0 = success, non-zero = failure
     */
    virtual int SCAPI SetCurrentLanguage(int LangCode, int DialectCode) = 0;
    virtual int SCAPI GetCurrentLanguage(int &LangCode, int &DialectCode) = 0;

    /* Get the list of dictionaries available */
    virtual int SCAPI GetNumOfDictionaries() = 0;
    /* Get the language and dialect id for an available dictionary.
     * Index = 0-based index into the list of available dictionaries.
     * Returns: 0 = success, non-zero = failure.
     */
    virtual int SCAPI GetDictionaryLanguage(int Index, int &LangCode, int &DialectCode) = 0;

    /* Called by the client application to initialize a buffer for spell checking.
     * It returns immediately without parsing the buffer. The client controls parsing of
     * the buffer by calling GetNextMisspelledWord() and SetNewBuf().
     * Return:  0 = success, non-zero = error
     */
    virtual int SCAPI SetBuf(const char *pBuf) = 0;

    /* Initialize a buffer with selection */
    virtual int SCAPI SetBuf(const char *pBuf, unsigned long SelStart, unsigned long SelEnd) = 0;

    /* Replace the current mispelled word with a new word */
    virtual int SCAPI ReplaceMisspelledWord(const char *NewWord, int AllInstances) = 0;

    /* Get the size of the current buffer */
    virtual unsigned long SCAPI GetBufSize() = 0;

    /* Copy the current buffer */
    virtual int SCAPI GetBuf(char *pBuf, unsigned long BufSize) = 0;

    /* Called by the client application to parse the buffer and return the next misspelled
     * word in the buffer.
     * Return:  0 = found a misspelled word.
     *              *Offset = Offset of the word from the beginning of the buffer
     *              *Len = Length of the word
     *          non-zero = no more misspelled word
     */
    virtual int SCAPI GetNextMisspelledWord(unsigned long &Offset, unsigned long &Len) = 0;

    /* The orginal buffer was changed by the client.
     * ReparseFromStart = 1 - reparse the new buffer from the beginning
     *                  = 0 - parse from the last offset into the original buffer
     */
    virtual void SCAPI SetNewBuf(const char *pBuf, int ReparseFromStart) = 0;

	/* Called by the client application to spell check a work.
     * Return:	1 = valid word, 0 = not in dictionary
     */
    virtual int SCAPI CheckWord(const char *pWord) = 0;

    /* Get the number of possible alternatives found in the dictionary for the input word. */
    virtual int SCAPI GetNumAlternatives(const char *pWord) = 0;

    /* Get an alternative string. The "Index" is zero based.
     * Return:  0 = success, -1 = error(bad Index), 
     *          +ve value = BufSize too small, size needed
     */
    virtual int SCAPI GetAlternative(int Index, char *pBuf, unsigned int BufSize) = 0;

    /* The following functions interact with the personal database */

    /* Add a word to the personal dictionary */
    virtual int SCAPI AddWordToPersonalDictionary(const char *pWord) = 0;

    /* Remove a word from the personal dictionary */
    virtual int SCAPI RemoveWordFromPersonalDictionary(const char *pWord) = 0;

    /* Ignore all references to a word in the current session */
    virtual int SCAPI IgnoreWord(const char *pWord) = 0;

    /* GetFirstPersonalDictionaryWord & GetNextPersonalDictionaryWord
     * These functions retrieve words in the personal dictionary
     * Returns:	0   = success, pBuf contains the next word
     *          -1  = no more words 
     *          +ve = required buffer size. Size passed is too small.
     */
    virtual int SCAPI GetFirstPersonalDictionaryWord(char *pBuf, int BufSize) = 0;
    virtual int SCAPI GetNextPersonalDictionaryWord(char *pBuf, int BufSize) = 0;

    /* Resets the contents of the personal dictionary */
    virtual int SCAPI ResetPersonalDictionary() = 0;

    /* destructor */
    virtual ~ISpellChecker() {};
};

/* Exported library functions to create and destroy ISpellChecker objects. */

extern "C" 
{

ISpellChecker * SCAPI SC_Create();
void SCAPI SC_Destroy(ISpellChecker *pSpellChecker);

}

#endif
