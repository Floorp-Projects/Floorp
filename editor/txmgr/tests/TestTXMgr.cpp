/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsTransactionManager.h"

static PRInt32 sConstructorCount     = 0;
static PRInt32 sDestructorCount      = 0;
static PRInt32 *sDestructorOrderArr  = 0;
static PRInt32 sDoCount              = 0;
static PRInt32 *sDoOrderArr          = 0;
static PRInt32 sUndoCount            = 0;
static PRInt32 *sUndoOrderArr        = 0;
static PRInt32 sRedoCount            = 0;
static PRInt32 *sRedoOrderArr        = 0;

// #define ENABLE_DEBUG_PRINTFS 1

static PRInt32 sSimpleTestDestructorOrderArr[] = {
         2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21,  1, 22, 23, 24, 25, 26, 27, 28, 29, 30,
        31, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 41, 40, 62, 39, 38, 37, 36, 35, 34,
        33, 32, 68, 71, 70, 69, 67, 66, 65, 64, 63 };

static PRInt32 sSimpleTestDoOrderArr[] = {
         1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
        46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
        61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71 };

static PRInt32 sSimpleTestUndoOrderArr[] = {
        41, 40, 39, 38, 62, 39, 38, 37, 69, 71, 70 };

static PRInt32 sSimpleTestRedoOrderArr[] = {
        38, 39, 70 };

PRInt32 sAggregateTestDestructorOrderArr[] = {
         14,  13,  12,  11,  10,   9,   8,  21,  20,  19,  18,  17,  16,  15,
         28,  27,  26,  25,  24,  23,  22,  35,  34,  33,  32,  31,  30,  29,
         42,  41,  40,  39,  38,  37,  36,  49,  48,  47,  46,  45,  44,  43,
         56,  55,  54,  53,  52,  51,  50,  63,  62,  61,  60,  59,  58,  57,
         70,  69,  68,  67,  66,  65,  64,  77,  76,  75,  74,  73,  72,  71,
         84,  83,  82,  81,  80,  79,  78,  91,  90,  89,  88,  87,  86,  85,
         98,  97,  96,  95,  94,  93,  92, 105, 104, 103, 102, 101, 100,  99,
        112, 111, 110, 109, 108, 107, 106, 119, 118, 117, 116, 115, 114, 113,
        126, 125, 124, 123, 122, 121, 120, 133, 132, 131, 130, 129, 128, 127,
        140, 139, 138, 137, 136, 135, 134, 147, 146, 145, 144, 143, 142, 141,
          7,   6,   5,   4,   3,   2,   1, 154, 153, 152, 151, 150, 149, 148,
        161, 160, 159, 158, 157, 156, 155, 168, 167, 166, 165, 164, 163, 162,
        175, 174, 173, 172, 171, 170, 169, 182, 181, 180, 179, 178, 177, 176,
        189, 188, 187, 186, 185, 184, 183, 196, 195, 194, 193, 192, 191, 190,
        203, 202, 201, 200, 199, 198, 197, 210, 209, 208, 207, 206, 205, 204,
        217, 216, 215, 214, 213, 212, 211, 294, 293, 292, 291, 290, 289, 288,
        301, 300, 299, 298, 297, 296, 295, 308, 307, 306, 305, 304, 303, 302,
        315, 314, 313, 312, 311, 310, 309, 322, 321, 320, 319, 318, 317, 316,
        329, 328, 327, 326, 325, 324, 323, 336, 335, 334, 333, 332, 331, 330,
        343, 342, 341, 340, 339, 338, 337, 350, 349, 348, 347, 346, 345, 344,
        357, 356, 355, 354, 353, 352, 351, 364, 363, 362, 361, 360, 359, 358,
        371, 370, 369, 368, 367, 366, 365, 378, 377, 376, 375, 374, 373, 372,
        385, 384, 383, 382, 381, 380, 379, 392, 391, 390, 389, 388, 387, 386,
        399, 398, 397, 396, 395, 394, 393, 406, 405, 404, 403, 402, 401, 400,
        413, 412, 411, 410, 409, 408, 407, 420, 419, 418, 417, 416, 415, 414,
        427, 426, 425, 424, 423, 422, 421, 287, 286, 285, 284, 283, 282, 281,
        280, 279, 278, 277, 276, 275, 274, 434, 433, 432, 431, 430, 429, 428,
        273, 272, 271, 270, 269, 268, 267, 266, 265, 264, 263, 262, 261, 260,
        259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 247, 246,
        245, 244, 243, 242, 241, 240, 239, 238, 237, 236, 235, 234, 233, 232,
        231, 230, 229, 228, 227, 226, 225, 224, 223, 222, 221, 220, 219, 218,
        472, 471, 470, 493, 492, 491, 490, 489, 488, 487, 486, 485, 484, 483,
        482, 481, 480, 479, 478, 477, 476, 475, 474, 473, 469, 468, 467, 466,
        465, 464, 463, 462, 461, 460, 459, 458, 457, 456, 455, 454, 453, 452,
        451, 450, 449, 448, 447, 446, 445, 444, 443, 442, 441, 440, 439, 438,
        437, 436, 435 };

PRInt32 sAggregateTestDoOrderArr[] = {
          1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
         15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
         29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
         43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
         57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,
         71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,
         85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,
         99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
        113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
        127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140,
        141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154,
        155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168,
        169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182,
        183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196,
        197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210,
        211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
        225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,
        239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252,
        253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266,
        267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280,
        281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294,
        295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 308,
        309, 310, 311, 312, 313, 314, 315, 316, 317, 318, 319, 320, 321, 322,
        323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336,
        337, 338, 339, 340, 341, 342, 343, 344, 345, 346, 347, 348, 349, 350,
        351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364,
        365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378,
        379, 380, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390, 391, 392,
        393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404, 405, 406,
        407, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420,
        421, 422, 423, 424, 425, 426, 427, 428, 429, 430, 431, 432, 433, 434,
        435, 436, 437, 438, 439, 440, 441, 442, 443, 444, 445, 446, 447, 448,
        449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 460, 461, 462,
        463, 464, 465, 466, 467, 468, 469, 470, 471, 472, 473, 474, 475, 476,
        477, 478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490,
        491, 492, 493 };

PRInt32 sAggregateTestUndoOrderArr[] = {
        287, 286, 285, 284, 283, 282, 281, 280, 279, 278, 277, 276, 275, 274,
        273, 272, 271, 270, 269, 268, 267, 266, 265, 264, 263, 262, 261, 260,
        434, 433, 432, 431, 430, 429, 428, 273, 272, 271, 270, 269, 268, 267,
        266, 265, 264, 263, 262, 261, 260, 259, 258, 257, 256, 255, 254, 253,
        479, 478, 477, 476, 475, 493, 492, 491, 490, 489, 488, 487, 486, 485,
        484, 483, 482, 481, 480, 485, 484, 483, 482, 481, 480 };

PRInt32 sAggregateTestRedoOrderArr[] = {
        260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273,
        476, 477, 478, 479, 480, 481, 482, 483, 484, 485, 486 };

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionIID, NS_ITRANSACTION_IID);
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);
static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);

class ConsoleOutput : public nsIOutputStream
{
public:
  ConsoleOutput()  {}
  virtual ~ConsoleOutput() {}

  NS_DECL_ISUPPORTS

  nsresult Close(void) {}
  nsresult Write(const char *str, PRInt32 offset, PRInt32 len, PRInt32 *wcnt)
  {
    *wcnt = fwrite(&str[offset], 1, len, stdout);
    fflush(stdout);
    return NS_OK;
  }
};

NS_IMPL_ADDREF(ConsoleOutput)
NS_IMPL_RELEASE(ConsoleOutput)

nsresult
ConsoleOutput::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIOutputStreamIID)) {
    *aInstancePtr = (void*)(nsITransaction*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

ConsoleOutput console;

class TestTransaction : public nsITransaction
{
public:

  TestTransaction() : mRefCnt(0) {}
  virtual ~TestTransaction()     {}

  NS_DECL_ISUPPORTS
};

NS_IMPL_ADDREF(TestTransaction)
NS_IMPL_RELEASE(TestTransaction)

nsresult
TestTransaction::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITransactionIID)) {
    *aInstancePtr = (void*)(nsITransaction*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

class SimpleTransaction : public TestTransaction
{
public:

#define NONE_FLAG               0
#define THROWS_DO_ERROR_FLAG    1
#define THROWS_UNDO_ERROR_FLAG  2
#define THROWS_REDO_ERROR_FLAG  4
#define MERGE_FLAG              8
#define TRANSIENT_FLAG         16
#define ALL_ERROR_FLAGS        (THROWS_DO_ERROR_FLAG|THROWS_UNDO_ERROR_FLAG|THROWS_REDO_ERROR_FLAG)

  PRInt32 mVal;
  PRInt32 mFlags;

  SimpleTransaction(PRInt32 aFlags=NONE_FLAG)
                    : mVal(++sConstructorCount), mFlags(aFlags)
  {}

  virtual ~SimpleTransaction()
  {
    //
    // Make sure transactions are being destroyed in the order we expect!
    // Notice that we don't check to see if we go past the end of the array.
    // This is done on purpose since we want to crash if the order array is out
    // of date.
    //
    if (sDestructorOrderArr && mVal != sDestructorOrderArr[sDestructorCount]) {
      printf("ERROR: ~SimpleTransaction expected %d got %d.\n",
             mVal, sDestructorOrderArr[sDestructorCount]);
      exit(NS_ERROR_FAILURE);
    }

    ++sDestructorCount;

#ifdef ENABLE_DEBUG_PRINTFS
    printf("\n~SimpleTransaction: %d - 0x%.8x\n", mVal, this);
#endif // ENABLE_DEBUG_PRINTFS

    mVal = -1;
  }

  virtual nsresult Do()
  {
    //
    // Make sure Do() is called in the order we expect!
    // Notice that we don't check to see if we go past the end of the array.
    // This is done on purpose since we want to crash if the order array is out
    // of date.
    //
    if (sDoOrderArr && mVal != sDoOrderArr[sDoCount]) {
      printf("ERROR: ~SimpleTransaction expected %d got %d.\n",
             mVal, sDoOrderArr[sDoCount]);
      exit(NS_ERROR_FAILURE);
    }

    ++sDoCount;

#ifdef ENABLE_DEBUG_PRINTFS
    printf("\nSimpleTransaction.Do: %d - 0x%.8x\n", mVal, this);
#endif // ENABLE_DEBUG_PRINTFS

    return (mFlags & THROWS_DO_ERROR_FLAG) ? NS_ERROR_FAILURE : NS_OK;
  }

  virtual nsresult Undo()
  {
    //
    // Make sure Undo() is called in the order we expect!
    // Notice that we don't check to see if we go past the end of the array.
    // This is done on purpose since we want to crash if the order array is out
    // of date.
    //
    if (sUndoOrderArr && mVal != sUndoOrderArr[sUndoCount]) {
      printf("ERROR: ~SimpleTransaction expected %d got %d.\n",
             mVal, sUndoOrderArr[sUndoCount]);
      exit(NS_ERROR_FAILURE);
    }

    ++sUndoCount;

#ifdef ENABLE_DEBUG_PRINTFS
    printf("\nSimpleTransaction.Undo: %d - 0x%.8x\n", mVal, this);
#endif // ENABLE_DEBUG_PRINTFS

    return (mFlags & THROWS_UNDO_ERROR_FLAG) ? NS_ERROR_FAILURE : NS_OK;
  }

  virtual nsresult Redo()
  {
    //
    // Make sure Redo() is called in the order we expect!
    // Notice that we don't check to see if we go past the end of the array.
    // This is done on purpose since we want to crash if the order array is out
    // of date.
    //
    if (sRedoOrderArr && mVal != sRedoOrderArr[sRedoCount]) {
      printf("ERROR: ~SimpleTransaction expected %d got %d.\n",
             mVal, sRedoOrderArr[sRedoCount]);
      exit(NS_ERROR_FAILURE);
    }

    ++sRedoCount;

#ifdef ENABLE_DEBUG_PRINTFS
    printf("\nSimpleTransaction.Redo: %d - 0x%.8x\n", mVal, this);
#endif // ENABLE_DEBUG_PRINTFS

    return (mFlags & THROWS_REDO_ERROR_FLAG) ? NS_ERROR_FAILURE : NS_OK;
  }

  virtual nsresult GetIsTransient(PRBool *aIsTransient)
  {
    if (aIsTransient)
      *aIsTransient = (mFlags & TRANSIENT_FLAG) ? PR_TRUE : PR_FALSE;

    return NS_OK;
  }

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
  {
    if (aDidMerge)
      *aDidMerge = (mFlags & MERGE_FLAG) ? PR_TRUE : PR_FALSE;

    return NS_OK;
  }

  virtual nsresult Write(nsIOutputStream *aOutputStream)
  {
    char buf[256];
    PRInt32 amt;

    sprintf(buf, "Transaction: %d - 0x%.8x\n", mVal, this);
    return aOutputStream->Write(buf, 0, strlen(buf), &amt);
  }

  virtual nsresult GetUndoString(nsString **aString)
  {
    *aString = 0;
    return NS_OK;
  }

  virtual nsresult GetRedoString(nsString **aString)
  {
    *aString = 0;
    return NS_OK;
  }

};

class AggregateTransaction : public SimpleTransaction
{
private:

  AggregateTransaction(nsITransactionManager *aTXMgr, PRInt32 aLevel,
                       PRInt32 aNumber, PRInt32 aMaxLevel,
                       PRInt32 aNumChildrenPerNode,
                       PRInt32 aFlags)
  {
    mLevel              = aLevel;
    mNumber             = aNumber;
    mTXMgr              = aTXMgr;
    mFlags              = aFlags & (~ALL_ERROR_FLAGS);
    mErrorFlags         = aFlags & ALL_ERROR_FLAGS;
    mTXMgr              = aTXMgr;
    mMaxLevel           = aMaxLevel;
    mNumChildrenPerNode = aNumChildrenPerNode;
  }


public:
  nsITransactionManager *mTXMgr;

  PRInt32 mLevel;
  PRInt32 mNumber;
  PRInt32 mErrorFlags;

  PRInt32 mMaxLevel;
  PRInt32 mNumChildrenPerNode;

  AggregateTransaction(nsITransactionManager *aTXMgr,
                       PRInt32 aMaxLevel, PRInt32 aNumChildrenPerNode,
                       PRInt32 aFlags=NONE_FLAG)
  {
    mLevel              = 1;
    mNumber             = 1;
    mFlags              = aFlags & (~ALL_ERROR_FLAGS);
    mErrorFlags         = aFlags & ALL_ERROR_FLAGS;
    mTXMgr              = aTXMgr;
    mMaxLevel           = aMaxLevel;
    mNumChildrenPerNode = aNumChildrenPerNode;
  }

  virtual ~AggregateTransaction()
  {
    // printf("~AggregateTransaction(0x%.8x) - %3d (%3d)\n", this, mLevel, mVal);
  }

  virtual nsresult Do()
  {
    if (mLevel >= mMaxLevel) {
      // Only leaf nodes can throw errors!
      mFlags |= mErrorFlags;
    }

    nsresult result = SimpleTransaction::Do();

    if (!NS_SUCCEEDED(result)) {
      // printf("ERROR: QueryInterface() failed for transaction level %d. (%d)\n",
      //       mLevel, result);
      return result;
    }

    if (mLevel >= mMaxLevel)
      return NS_OK;

    PRInt32 cLevel = mLevel + 1;

    for (int i = 1; i <= mNumChildrenPerNode; i++) {
      PRInt32 flags = mErrorFlags & THROWS_DO_ERROR_FLAG;

      if ((mErrorFlags & THROWS_REDO_ERROR_FLAG) && i == mNumChildrenPerNode) {
        // Make the rightmost leaf transaction throw the error!
        flags = THROWS_REDO_ERROR_FLAG;
        mErrorFlags = mErrorFlags & (~THROWS_REDO_ERROR_FLAG);
      }
      else if ((mErrorFlags & THROWS_UNDO_ERROR_FLAG)
               && i == 1) {
        // Make the leftmost leaf transaction throw the error!
        flags = THROWS_UNDO_ERROR_FLAG;
        mErrorFlags = mErrorFlags & (~THROWS_UNDO_ERROR_FLAG);
      }

      AggregateTransaction *tximpl =
              new AggregateTransaction(mTXMgr, cLevel, i, mMaxLevel,
                                       mNumChildrenPerNode, flags);

      nsITransaction *tx = 0;
      result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: QueryInterface() failed for transaction %d, level %d. (%d)\n",
               i, mLevel, result);
        return result;
      }

      result = mTXMgr->Do(tx);

      if (!NS_SUCCEEDED(result)) {
        // printf("ERROR: Failed to execute transaction %d, level %d. (%d)\n",
        //        i, mLevel, result);
        tx->Release();
        return result;
      }

      tx->Release();
    }

    return result;
  }
};

void
reset_globals()
{
  sConstructorCount   = 0;

  sDestructorCount    = 0;
  sDestructorOrderArr = 0;

  sDoCount            = 0;
  sDoOrderArr         = 0;

  sUndoCount          = 0;
  sUndoOrderArr       = 0;

  sRedoCount          = 0;
  sRedoOrderArr       = 0;
}

nsresult
simple_test()
{
  /*******************************************************************
   *
   * Initialize globals for test.
   *
   *******************************************************************/

  reset_globals();
  sDestructorOrderArr = sSimpleTestDestructorOrderArr;
  sDoOrderArr         = sSimpleTestDoOrderArr;
  sUndoOrderArr       = sSimpleTestUndoOrderArr;
  sRedoOrderArr       = sSimpleTestRedoOrderArr;

  /*******************************************************************
   *
   * Create a transaction manager implementation:
   *
   *******************************************************************/

  printf("Create a transaction manager with 10 levels of undo ... ");

  PRInt32 i;
  nsTransactionManager  *mgrimpl = new nsTransactionManager(10);
  nsITransactionManager  *mgr    = 0;
  nsITransaction *tx             = 0;
  SimpleTransaction *tximpl      = 0;
  nsITransaction *u1 = 0, *u2    = 0;
  nsITransaction *r1 = 0, *r2    = 0;


  if (!mgrimpl) {
    printf("ERROR: Failed to create nsTransactionManager.\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test QueryInterface():
   *
   *******************************************************************/

  printf("Get the nsITransactionManager interface ... ");

  nsresult result = mgrimpl->QueryInterface(kITransactionManagerIID,
                                            (void **)&mgr);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to get TransactionManager interface. (%d)\n", result);
    return result;
  }

  if (!mgr) {
    printf("ERROR: QueryInterface() returned NULL pointer.\n");
    return NS_ERROR_NULL_POINTER;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Do() with a null transaction:
   *
   *******************************************************************/

  printf("Call Do() with null transaction ... ");
  result = mgr->Do(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NULL_POINTER) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Undo() with an empty undo stack:
   *
   *******************************************************************/

  printf("Call Undo() with empty undo stack ... ");
  result = mgr->Undo();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Undo on empty undo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Redo() with an empty redo stack:
   *
   *******************************************************************/

  printf("Call Redo() with empty redo stack ... ");
  result = mgr->Redo();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Redo on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Clear() with empty undo and redo stacks:
   *
   *******************************************************************/

  printf("Call Clear() with empty undo and redo stack ... ");
  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear on empty undo and redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call GetNumberOfUndoItems() with an empty undo stack:
   *
   *******************************************************************/

  PRInt32 numitems = 0;

  printf("Call GetNumberOfUndoItems() with empty undo stack ... ");
  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on empty undo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfUndoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call GetNumberOfRedoItems() with an empty redo stack:
   *
   *******************************************************************/

  printf("Call GetNumberOfRedoItems() with empty redo stack ... ");
  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call PeekUndoStack() with an empty undo stack:
   *
   *******************************************************************/

  printf("Call PeekUndoStack() with empty undo stack ... ");

  tx = 0;
  result = mgr->PeekUndoStack(&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: PeekUndoStack() on empty undo stack failed. (%d)\n", result);
    return result;
  }

  if (tx != 0) {
    printf("ERROR: PeekUndoStack() on empty undo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call PeekRedoStack() with an empty undo stack:
   *
   *******************************************************************/

  printf("Call PeekRedoStack() with empty undo stack ... ");

  tx = 0;
  result = mgr->PeekRedoStack(&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: PeekRedoStack() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  if (tx != 0) {
    printf("ERROR: PeekRedoStack() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Write() with a null output stream:
   *
   *******************************************************************/

  printf("Call Write() with null output stream ... ");

  result = mgr->Write(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NULL_POINTER) {
    printf("ERROR: Write() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call AddListener() with a null listener pointer:
   *
   *******************************************************************/

  printf("Call AddListener() with null listener ... ");

  result = mgr->AddListener(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NOT_IMPLEMENTED) {
    printf("ERROR: AddListener() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call RemoveListener() with a null listener pointer:
   *
   *******************************************************************/

  printf("Call RemoveListener() with null listener ... ");

  result = mgr->RemoveListener(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NOT_IMPLEMENTED) {
    printf("ERROR: RemoveListener() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test coalescing by executing a transaction that can merge any
   * command into itself. Then execute 20 transaction. Afterwards,
   * we should still have the first transaction sitting on the undo
   * stack.
   *
   *******************************************************************/

  printf("Test coalescing of transactions ... ");

  tximpl = new SimpleTransaction(MERGE_FLAG);
  tx = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for initial transaction. (%d)\n",
           result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to execute initial transaction. (%d)\n", result);
    return result;
  }

  tx->Release();

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != tx) {
    printf("ERROR: Top of undo stack is different!. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  for (i = 1; i <= 20; i++) {
    tximpl = new SimpleTransaction();

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 1 item failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 1) {
    printf("ERROR: GetNumberOfUndoItems() expected 1 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 20 transactions. Afterwards, we should have 10
   * transactions on the undo stack:
   *
   *******************************************************************/

  printf("Execute 20 simple transactions ... ");

  for (i = 1; i <= 20; i++) {
    tximpl = new SimpleTransaction();

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 10 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 10) {
    printf("ERROR: GetNumberOfUndoItems() expected 10 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 20 transient transactions. Afterwards, we should still
   * have the same 10 transactions on the undo stack:
   *
   *******************************************************************/

  printf("Execute 20 transient transactions ... ");

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  for (i = 1; i <= 20; i++) {
    tximpl = new SimpleTransaction(TRANSIENT_FLAG);

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 10 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 10) {
    printf("ERROR: GetNumberOfUndoItems() expected 10 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Undo 4 transactions. Afterwards, we should have 6 transactions
   * on the undo stack, and 4 on the redo stack:
   *
   *******************************************************************/

  printf("Undo 4 transactions ... ");

  for (i = 1; i <= 4; i++) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 4) {
    printf("ERROR: GetNumberOfRedoItems() expected 4 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Redo 2 transactions. Afterwards, we should have 8 transactions
   * on the undo stack, and 2 on the redo stack:
   *
   *******************************************************************/

  printf("Redo 2 transactions ... ");

  for (i = 1; i <= 2; ++i) {
    result = mgr->Redo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to redo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 8 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 8) {
    printf("ERROR: GetNumberOfUndoItems() expected 8 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 2 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 2) {
    printf("ERROR: GetNumberOfRedoItems() expected 2 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute a new transaction. The redo stack should get pruned!
   *
   *******************************************************************/

  printf("Check if new transactions prune the redo stack ... ");

  tximpl = new SimpleTransaction();
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);
  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to execute transaction. (%d)\n", result);
    return result;
  }

  tx->Release();

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 9 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 9) {
    printf("ERROR: GetNumberOfUndoItems() expected 9 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 0 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Undo 4 transactions then clear the undo and redo stacks.
   *
   *******************************************************************/

  printf("Undo 4 transactions then clear the undo and redo stacks ... ");

  for (i = 1; i <= 4; ++i) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 4 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 4) {
    printf("ERROR: GetNumberOfRedoItems() expected 4 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear() failed. (%d)\n",
           result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on cleared undo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfUndoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty cleared stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 5 transactions.
   *
   *******************************************************************/

  printf("Execute 5 simple transactions ... ");

  for (i = 1; i <= 5; i++) {
    tximpl = new SimpleTransaction();

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Do() error:
   *
   *******************************************************************/

  printf("Test transaction Do() error ... ");

  tximpl = new SimpleTransaction(THROWS_DO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Undo() error:
   *
   *******************************************************************/

  printf("Test transaction Undo() error ... ");

  tximpl = new SimpleTransaction(THROWS_UNDO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Undo();

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Undo() returned unexpected error. (%d)\n", result);
    return result;
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Redo() error:
   *
   *******************************************************************/

  printf("Test transaction Redo() error ... ");

  tximpl = new SimpleTransaction(THROWS_REDO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for RedoErrorTransaction. (%d)\n",
           result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  //
  // Execute a normal transaction to be used in a later test:
  //

  tximpl = new SimpleTransaction();
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  //
  // Undo the 2 transactions just executed.
  //

  for (i = 1; i <= 2; ++i) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  //
  // The RedoErrorTransaction should now be at the top of the redo stack!
  //

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Redo();

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Redo() returned unexpected error. (%d)\n", result);
    return result;
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 2) {
    printf("ERROR: GetNumberOfRedoItems() expected 2 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Release the transaction manager. Any transactions on the undo
   * and redo stack should automatically be released:
   *
   *******************************************************************/

  // mgr->Write(&console);

  printf("Release the transaction manager ... ");

  result = mgr->Release();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Make sure number of transactions created matches number of
   * transactions destroyed!
   *
   *******************************************************************/

  printf("Number of transactions created and destroyed match ... ");

  if (sConstructorCount != sDestructorCount) {
    printf("ERROR: Transaction constructor count (%d) != destructor count (%d).\n",
           sConstructorCount, sDestructorCount);
    return result;
  }

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  return NS_OK;
}

nsresult
aggregation_test()
{
  /*******************************************************************
   *
   * Initialize globals for test.
   *
   *******************************************************************/

  reset_globals();
  sDestructorOrderArr = sAggregateTestDestructorOrderArr;
  sDoOrderArr         = sAggregateTestDoOrderArr;
  sUndoOrderArr       = sAggregateTestUndoOrderArr;
  sRedoOrderArr       = sAggregateTestRedoOrderArr;

  /*******************************************************************
   *
   * Create a transaction manager implementation:
   *
   *******************************************************************/

  printf("Create a transaction manager with 10 levels of undo ... ");

  PRInt32 i, numitems;
  nsTransactionManager  *mgrimpl = new nsTransactionManager(10);
  nsITransactionManager  *mgr    = 0;
  nsITransaction *tx             = 0;
  AggregateTransaction *tximpl   = 0;
  nsITransaction *u1 = 0, *u2 = 0;
  nsITransaction *r1 = 0, *r2 = 0;


  if (!mgrimpl) {
    printf("ERROR: Failed to create nsTransactionManager.\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test QueryInterface():
   *
   *******************************************************************/

  printf("Get the nsITransactionManager interface ... ");

  nsresult result = mgrimpl->QueryInterface(kITransactionManagerIID,
                                            (void **)&mgr);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to get TransactionManager interface. (%d)\n", result);
    return result;
  }

  if (!mgr) {
    printf("ERROR: QueryInterface() returned NULL pointer.\n");
    return NS_ERROR_NULL_POINTER;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test coalescing by executing a transaction that can merge any
   * command into itself. Then execute 20 transaction. Afterwards,
   * we should still have the first transaction sitting on the undo
   * stack.
   *
   *******************************************************************/

  printf("Test coalescing of transactions ... ");

  tximpl = new AggregateTransaction(mgr, 3, 2, MERGE_FLAG);
  tx = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for initial transaction. (%d)\n",
           result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to execute initial transaction. (%d)\n", result);
    return result;
  }

  tx->Release();

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != tx) {
    printf("ERROR: Top of undo stack is different!. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  for (i = 1; i <= 20; i++) {
    tximpl = new AggregateTransaction(mgr, 3, 2);

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 1 item failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 1) {
    printf("ERROR: GetNumberOfUndoItems() expected 1 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 20 transactions. Afterwards, we should have 10
   * transactions on the undo stack:
   *
   *******************************************************************/

  printf("Execute 20 aggregate transactions ... ");

  for (i = 1; i <= 20; i++) {
    tximpl = new AggregateTransaction(mgr, 3, 2);

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 10 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 10) {
    printf("ERROR: GetNumberOfUndoItems() expected 10 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 20 transient transactions. Afterwards, we should still
   * have the same 10 transactions on the undo stack:
   *
   *******************************************************************/

  printf("Execute 20 transient aggregate transactions ... ");

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  for (i = 1; i <= 20; i++) {
    tximpl = new AggregateTransaction(mgr, 3, 2, TRANSIENT_FLAG);

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 10 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 10) {
    printf("ERROR: GetNumberOfUndoItems() expected 10 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Undo 4 transactions. Afterwards, we should have 6 transactions
   * on the undo stack, and 4 on the redo stack:
   *
   *******************************************************************/

  printf("Undo 4 transactions ... ");

  for (i = 1; i <= 4; i++) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 4) {
    printf("ERROR: GetNumberOfRedoItems() expected 4 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Redo 2 transactions. Afterwards, we should have 8 transactions
   * on the undo stack, and 2 on the redo stack:
   *
   *******************************************************************/

  printf("Redo 2 transactions ... ");

  for (i = 1; i <= 2; ++i) {
    result = mgr->Redo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to redo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 8 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 8) {
    printf("ERROR: GetNumberOfUndoItems() expected 8 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 2 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 2) {
    printf("ERROR: GetNumberOfRedoItems() expected 2 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute a new transaction. The redo stack should get pruned!
   *
   *******************************************************************/

  printf("Check if new aggregate transactions prune the redo stack ... ");

  tximpl = new AggregateTransaction(mgr, 3, 2);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);
  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to execute transaction. (%d)\n", result);
    return result;
  }

  tx->Release();

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 9 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 9) {
    printf("ERROR: GetNumberOfUndoItems() expected 9 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 0 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Undo 4 transactions then clear the undo and redo stacks.
   *
   *******************************************************************/

  printf("Undo 4 transactions then clear the undo and redo stacks ... ");

  for (i = 1; i <= 4; ++i) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 4 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 4) {
    printf("ERROR: GetNumberOfRedoItems() expected 4 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear() failed. (%d)\n",
           result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on cleared undo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfUndoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty cleared stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 5 transactions.
   *
   *******************************************************************/

  printf("Execute 5 aggregate transactions ... ");

  for (i = 1; i <= 5; i++) {
    tximpl = new AggregateTransaction(mgr, 3, 2);

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Do() error:
   *
   *******************************************************************/

  printf("Test aggregate transaction Do() error ... ");

  tximpl = new AggregateTransaction(mgr, 3, 2, THROWS_DO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Undo() error:
   *
   *******************************************************************/

  printf("Test aggregate transaction Undo() error ... ");

  tximpl = new AggregateTransaction(mgr, 3, 2, THROWS_UNDO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Undo();

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Undo() returned unexpected error. (%d)\n", result);
    return result;
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Redo() error:
   *
   *******************************************************************/

  printf("Test aggregate transaction Redo() error ... ");

  tximpl = new AggregateTransaction(mgr, 3, 2, THROWS_REDO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for RedoErrorTransaction. (%d)\n",
           result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  //
  // Execute a normal transaction to be used in a later test:
  //

  tximpl = new AggregateTransaction(mgr, 3, 2);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  //
  // Undo the 2 transactions just executed.
  //

  for (i = 1; i <= 2; ++i) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  //
  // The RedoErrorTransaction should now be at the top of the redo stack!
  //

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Redo();

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Redo() returned unexpected error. (%d)\n", result);
    return result;
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 2) {
    printf("ERROR: GetNumberOfRedoItems() expected 2 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Release the transaction manager. Any transactions on the undo
   * and redo stack should automatically be released:
   *
   *******************************************************************/

  // mgr->Write(&console);

  printf("Release the transaction manager ... ");

  result = mgr->Release();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Make sure number of transactions created matches number of
   * transactions destroyed!
   *
   *******************************************************************/

  printf("Number of transactions created and destroyed match ... ");

  if (sConstructorCount != sDestructorCount) {
    printf("ERROR: Transaction constructor count (%d) != destructor count (%d).\n",
           sConstructorCount, sDestructorCount);
    return result;
  }

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  return NS_OK;
}

nsresult
stress_test()
{
  /*******************************************************************
   *
   * Initialize globals for test.
   *
   *******************************************************************/

  reset_globals();

  /*******************************************************************
   *
   * Create a transaction manager:
   *
   *******************************************************************/

  printf("Simple transaction stress test (may take a while) ... ");
  fflush(stdout);

  PRInt32 i, j;
  nsTransactionManager  *mgrimpl = new nsTransactionManager();
  nsITransactionManager  *mgr    = 0;
  nsITransaction *tx             = 0;
  SimpleTransaction *tximpl      = 0;

  if (!mgrimpl) {
    printf("ERROR: Failed to create nsTransactionManager.\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult result = mgrimpl->QueryInterface(kITransactionManagerIID,
                                            (void **)&mgr);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to get TransactionManager interface. (%d)\n", result);
    return result;
  }

  if (!mgr) {
    printf("ERROR: QueryInterface() returned NULL pointer.\n");
    return NS_ERROR_NULL_POINTER;
  }

  //
  // A num == 1500 sends 1,125,750 transactions through the system!!
  //
  PRInt32 num = 1500;
                      
  for (i = 1; i <= num; i++) {
    /*******************************************************************
     *
     * Execute a bunch of transactions:
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      tximpl = new SimpleTransaction();

      tx = 0;
      result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
               i, result);
        return result;
      }

      result = mgr->Do(tx);
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
        return result;
      }

      tx->Release();
    }

    /*******************************************************************
     *
     * Undo all the transactions:
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      result = mgr->Undo();
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to undo transaction %d-%d. (%d)\n", i, j, result);
        return result;
      }
    }

    /*******************************************************************
     *
     * Redo all the transactions:
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      result = mgr->Redo();
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to redo transaction %d-%d. (%d)\n", i, j, result);
        return result;
      }
    }

    /*******************************************************************
     *
     * Undo all the transactions again so that they all end up on
     * the redo stack for pruning the next time we execute a new
     * transaction
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      result = mgr->Undo();
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to undo transaction %d-%d. (%d)\n", i, j, result);
        return result;
      }
    }
  }

  result = mgr->Release();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  // printf("%d  %d -  ", sConstructorCount, sDestructorCount);

  if (sConstructorCount != sDestructorCount) {
    printf("ERROR: Transaction constructor count (%d) != destructor count (%d).\n",
           sConstructorCount, sDestructorCount);
    return result;
  }

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  return NS_OK;
}

nsresult
aggregation_stress_test()
{
  /*******************************************************************
   *
   * Initialize globals for test.
   *
   *******************************************************************/

  reset_globals();

  /*******************************************************************
   *
   * Create a transaction manager:
   *
   *******************************************************************/

  printf("Aggregated transaction stress test (may take a while) ... ");
  fflush(stdout);

  PRInt32 i, j;
  nsTransactionManager  *mgrimpl = new nsTransactionManager();
  nsITransactionManager  *mgr    = 0;
  nsITransaction *tx             = 0;
  AggregateTransaction *tximpl   = 0;

  if (!mgrimpl) {
    printf("ERROR: Failed to create nsTransactionManager.\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult result = mgrimpl->QueryInterface(kITransactionManagerIID,
                                            (void **)&mgr);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to get TransactionManager interface. (%d)\n", result);
    return result;
  }

  if (!mgr) {
    printf("ERROR: QueryInterface() returned NULL pointer.\n");
    return NS_ERROR_NULL_POINTER;
  }

  //
  // A num == 500 sends 2,630,250 transactions through the system!!
  //
  PRInt32 num = 500;
                      
  for (i = 1; i <= num; i++) {
    /*******************************************************************
     *
     * Execute a bunch of transactions:
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      tximpl = new AggregateTransaction(mgr, 3, 4);

      tx = 0;
      result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
               i, result);
        return result;
      }

      result = mgr->Do(tx);
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
        return result;
      }

      tx->Release();
    }

    /*******************************************************************
     *
     * Undo all the transactions:
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      result = mgr->Undo();
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to undo transaction %d-%d. (%d)\n", i, j, result);
        return result;
      }
    }

    /*******************************************************************
     *
     * Redo all the transactions:
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      result = mgr->Redo();
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to redo transaction %d-%d. (%d)\n", i, j, result);
        return result;
      }
    }

    /*******************************************************************
     *
     * Undo all the transactions again so that they all end up on
     * the redo stack for pruning the next time we execute a new
     * transaction
     *
     *******************************************************************/

    for (j = 1; j <= i; j++) {
      result = mgr->Undo();
      if (!NS_SUCCEEDED(result)) {
        printf("ERROR: Failed to undo transaction %d-%d. (%d)\n", i, j, result);
        return result;
      }
    }
  }

  result = mgr->Release();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  // printf("%d  %d -  ", sConstructorCount, sDestructorCount);

  if (sConstructorCount != sDestructorCount) {
    printf("ERROR: Transaction constructor count (%d) != destructor count (%d).\n",
           sConstructorCount, sDestructorCount);
    return result;
  }

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  return NS_OK;
}

int
main (int argc, char *argv[])
{
  nsresult result;

  result = simple_test();

  if (!NS_SUCCEEDED(result))
    return result;

  result = aggregation_test();

  if (!NS_SUCCEEDED(result))
    return result;

  result = stress_test();

  if (!NS_SUCCEEDED(result))
    return result;

  result = aggregation_stress_test();

  if (!NS_SUCCEEDED(result))
    return result;

  return NS_OK;
}
