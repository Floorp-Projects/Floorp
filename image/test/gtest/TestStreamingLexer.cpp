/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/Vector.h"
#include "StreamingLexer.h"

using namespace mozilla;
using namespace mozilla::image;

enum class TestState
{
  ONE,
  TWO,
  THREE,
  UNBUFFERED,
  TRUNCATED_SUCCESS,
  TRUNCATED_FAILURE
};

void
CheckLexedData(const char* aData,
               size_t aLength,
               size_t aOffset,
               size_t aExpectedLength)
{
  EXPECT_TRUE(aLength == aExpectedLength);

  for (size_t i = 0; i < aLength; ++i) {
    EXPECT_EQ(aData[i], char(aOffset + i + 1));
  }
}

LexerTransition<TestState>
DoLex(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      CheckLexedData(aData, aLength, 0, 3);
      return Transition::To(TestState::TWO, 3);
    case TestState::TWO:
      CheckLexedData(aData, aLength, 3, 3);
      return Transition::To(TestState::THREE, 3);
    case TestState::THREE:
      CheckLexedData(aData, aLength, 6, 3);
      return Transition::TerminateSuccess();
    case TestState::TRUNCATED_SUCCESS:
      return Transition::TerminateSuccess();
    case TestState::TRUNCATED_FAILURE:
      return Transition::TerminateFailure();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithUnbuffered(TestState aState, const char* aData, size_t aLength,
                    Vector<char>& aUnbufferedVector)
{
  switch (aState) {
    case TestState::ONE:
      CheckLexedData(aData, aLength, 0, 3);
      return Transition::ToUnbuffered(TestState::TWO, TestState::UNBUFFERED, 3);
    case TestState::TWO:
      CheckLexedData(aUnbufferedVector.begin(), aUnbufferedVector.length(), 3, 3);
      return Transition::To(TestState::THREE, 3);
    case TestState::THREE:
      CheckLexedData(aData, aLength, 6, 3);
      return Transition::TerminateSuccess();
    case TestState::UNBUFFERED:
      EXPECT_TRUE(aLength <= 3);
      EXPECT_TRUE(aUnbufferedVector.append(aData, aLength));
      return Transition::ContinueUnbuffered(TestState::UNBUFFERED);
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithUnbufferedTerminate(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      CheckLexedData(aData, aLength, 0, 3);
      return Transition::ToUnbuffered(TestState::TWO, TestState::UNBUFFERED, 3);
    case TestState::UNBUFFERED:
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithYield(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      CheckLexedData(aData, aLength, 0, 3);
      return Transition::ToAfterYield(TestState::TWO);
    case TestState::TWO:
      CheckLexedData(aData, aLength, 0, 3);
      return Transition::To(TestState::THREE, 6);
    case TestState::THREE:
      CheckLexedData(aData, aLength, 3, 6);
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithTerminateAfterYield(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      CheckLexedData(aData, aLength, 0, 3);
      return Transition::ToAfterYield(TestState::TWO);
    case TestState::TWO:
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithZeroLengthStates(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      EXPECT_TRUE(aLength == 0);
      return Transition::To(TestState::TWO, 0);
    case TestState::TWO:
      EXPECT_TRUE(aLength == 0);
      return Transition::To(TestState::THREE, 9);
    case TestState::THREE:
      CheckLexedData(aData, aLength, 0, 9);
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithZeroLengthStatesAtEnd(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      CheckLexedData(aData, aLength, 0, 9);
      return Transition::To(TestState::TWO, 0);
    case TestState::TWO:
      EXPECT_TRUE(aLength == 0);
      return Transition::To(TestState::THREE, 0);
    case TestState::THREE:
      EXPECT_TRUE(aLength == 0);
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithZeroLengthYield(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      EXPECT_EQ(0u, aLength);
      return Transition::ToAfterYield(TestState::TWO);
    case TestState::TWO:
      EXPECT_EQ(0u, aLength);
      return Transition::To(TestState::THREE, 9);
    case TestState::THREE:
      CheckLexedData(aData, aLength, 0, 9);
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithZeroLengthStatesUnbuffered(TestState aState,
                                    const char* aData,
                                    size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      EXPECT_TRUE(aLength == 0);
      return Transition::ToUnbuffered(TestState::TWO, TestState::UNBUFFERED, 0);
    case TestState::TWO:
      EXPECT_TRUE(aLength == 0);
      return Transition::To(TestState::THREE, 9);
    case TestState::THREE:
      CheckLexedData(aData, aLength, 0, 9);
      return Transition::TerminateSuccess();
    case TestState::UNBUFFERED:
      ADD_FAILURE() << "Should not enter zero-length unbuffered state";
      return Transition::TerminateFailure();
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

LexerTransition<TestState>
DoLexWithZeroLengthStatesAfterUnbuffered(TestState aState,
                                         const char* aData,
                                         size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      EXPECT_TRUE(aLength == 0);
      return Transition::ToUnbuffered(TestState::TWO, TestState::UNBUFFERED, 9);
    case TestState::TWO:
      EXPECT_TRUE(aLength == 0);
      return Transition::To(TestState::THREE, 0);
    case TestState::THREE:
      EXPECT_TRUE(aLength == 0);
      return Transition::TerminateSuccess();
    case TestState::UNBUFFERED:
      CheckLexedData(aData, aLength, 0, 9);
      return Transition::ContinueUnbuffered(TestState::UNBUFFERED);
    default:
      MOZ_CRASH("Unexpected or unhandled TestState");
  }
}

class ImageStreamingLexer : public ::testing::Test
{
public:
  // Note that mLexer is configured to enter TerminalState::FAILURE immediately
  // if the input data is truncated. We don't expect that to happen in most
  // tests, so we want to detect that issue. If a test needs a different
  // behavior, we create a special StreamingLexer just for that test.
  ImageStreamingLexer()
    : mLexer(Transition::To(TestState::ONE, 3), Transition::TerminateFailure())
    , mSourceBuffer(new SourceBuffer)
    , mIterator(mSourceBuffer->Iterator())
    , mExpectNoResume(new ExpectNoResume)
    , mCountResumes(new CountResumes)
  { }

protected:
  void CheckTruncatedState(StreamingLexer<TestState>& aLexer,
                           TerminalState aExpectedTerminalState,
                           nsresult aCompletionStatus = NS_OK)
  {
    for (unsigned i = 0; i < 9; ++i) {
      if (i < 2) {
        mSourceBuffer->Append(mData + i, 1);
      } else if (i == 2) {
        mSourceBuffer->Complete(aCompletionStatus);
      }

      LexerResult result = aLexer.Lex(mIterator, mCountResumes, DoLex);

      if (i >= 2) {
        EXPECT_TRUE(result.is<TerminalState>());
        EXPECT_EQ(aExpectedTerminalState, result.as<TerminalState>());
      } else {
        EXPECT_TRUE(result.is<Yield>());
        EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
      }
    }

    EXPECT_EQ(2u, mCountResumes->Count());
  }

  AutoInitializeImageLib mInit;
  const char mData[9] { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  StreamingLexer<TestState> mLexer;
  RefPtr<SourceBuffer> mSourceBuffer;
  SourceBufferIterator mIterator;
  RefPtr<ExpectNoResume> mExpectNoResume;
  RefPtr<CountResumes> mCountResumes;
};

TEST_F(ImageStreamingLexer, ZeroLengthData)
{
  // Test a zero-length input.
  mSourceBuffer->Complete(NS_OK);

  LexerResult result = mLexer.Lex(mIterator, mExpectNoResume, DoLex);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::FAILURE, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, ZeroLengthDataUnbuffered)
{
  // Test a zero-length input.
  mSourceBuffer->Complete(NS_OK);

  // Create a special StreamingLexer for this test because we want the first
  // state to be unbuffered.
  StreamingLexer<TestState> lexer(Transition::ToUnbuffered(TestState::ONE,
                                                           TestState::UNBUFFERED,
                                                           sizeof(mData)),
                                  Transition::TerminateFailure());

  LexerResult result = lexer.Lex(mIterator, mExpectNoResume, DoLex);
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::FAILURE, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, StartWithTerminal)
{
  // Create a special StreamingLexer for this test because we want the first
  // state to be a terminal state. This doesn't really make sense, but we should
  // handle it.
  StreamingLexer<TestState> lexer(Transition::TerminateSuccess(),
                                  Transition::TerminateFailure());
  LexerResult result = lexer.Lex(mIterator, mExpectNoResume, DoLex);
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());

  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, SingleChunk)
{
  // Test delivering all the data at once.
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  LexerResult result = mLexer.Lex(mIterator, mExpectNoResume, DoLex);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, SingleChunkWithUnbuffered)
{
  Vector<char> unbufferedVector;

  // Test delivering all the data at once.
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  LexerResult result =
    mLexer.Lex(mIterator, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
  });

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, SingleChunkWithYield)
{
  // Test delivering all the data at once.
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  LexerResult result = mLexer.Lex(mIterator, mExpectNoResume, DoLexWithYield);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());

  result = mLexer.Lex(mIterator, mExpectNoResume, DoLexWithYield);
  ASSERT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, ChunkPerState)
{
  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0; i < 3; ++i) {
    mSourceBuffer->Append(mData + 3 * i, 3);
    LexerResult result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i == 2) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  EXPECT_EQ(2u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, ChunkPerStateWithUnbuffered)
{
  Vector<char> unbufferedVector;

  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0; i < 3; ++i) {
    mSourceBuffer->Append(mData + 3 * i, 3);
    LexerResult result =
      mLexer.Lex(mIterator, mCountResumes,
                 [&](TestState aState, const char* aData, size_t aLength) {
        return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
    });

    if (i == 2) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  EXPECT_EQ(2u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, ChunkPerStateWithYield)
{
  // Test delivering in perfectly-sized chunks, one per state.
  mSourceBuffer->Append(mData, 3);
  LexerResult result = mLexer.Lex(mIterator, mCountResumes, DoLexWithYield);
  EXPECT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());

  result = mLexer.Lex(mIterator, mCountResumes, DoLexWithYield);
  EXPECT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());

  mSourceBuffer->Append(mData + 3, 6);
  result = mLexer.Lex(mIterator, mCountResumes, DoLexWithYield);
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());

  EXPECT_EQ(1u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, ChunkPerStateWithUnbufferedYield)
{
  size_t unbufferedCallCount = 0;
  Vector<char> unbufferedVector;
  auto lexerFunc = [&](TestState aState, const char* aData, size_t aLength)
                     -> LexerTransition<TestState> {
    switch (aState) {
      case TestState::ONE:
        CheckLexedData(aData, aLength, 0, 3);
        return Transition::ToUnbuffered(TestState::TWO, TestState::UNBUFFERED, 3);
      case TestState::TWO:
        CheckLexedData(unbufferedVector.begin(), unbufferedVector.length(), 3, 3);
        return Transition::To(TestState::THREE, 3);
      case TestState::THREE:
        CheckLexedData(aData, aLength, 6, 3);
        return Transition::TerminateSuccess();
      case TestState::UNBUFFERED:
        switch (unbufferedCallCount) {
          case 0:
            CheckLexedData(aData, aLength, 3, 3);
            EXPECT_TRUE(unbufferedVector.append(aData, 2));
            unbufferedCallCount++;

            // Continue after yield, telling StreamingLexer we consumed 2 bytes.
            return Transition::ContinueUnbufferedAfterYield(TestState::UNBUFFERED, 2);

          case 1:
            CheckLexedData(aData, aLength, 5, 1);
            EXPECT_TRUE(unbufferedVector.append(aData, 1));
            unbufferedCallCount++;

            // Continue after yield, telling StreamingLexer we consumed 1 byte.
            // We should end up in the TWO state.
            return Transition::ContinueUnbuffered(TestState::UNBUFFERED);
        }
        ADD_FAILURE() << "Too many invocations of TestState::UNBUFFERED";
        return Transition::TerminateFailure();
      default:
        MOZ_CRASH("Unexpected or unhandled TestState");
    }
  };

  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0; i < 3; ++i) {
    mSourceBuffer->Append(mData + 3 * i, 3);
    LexerResult result = mLexer.Lex(mIterator, mCountResumes, lexerFunc);

    switch (i) {
      case 0:
        EXPECT_TRUE(result.is<Yield>());
        EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
        EXPECT_EQ(0u, unbufferedCallCount);
        break;

      case 1:
        EXPECT_TRUE(result.is<Yield>());
        EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());
        EXPECT_EQ(1u, unbufferedCallCount);

        result = mLexer.Lex(mIterator, mCountResumes, lexerFunc);
        EXPECT_TRUE(result.is<Yield>());
        EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
        EXPECT_EQ(2u, unbufferedCallCount);
        break;

      case 2:
        EXPECT_TRUE(result.is<TerminalState>());
        EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
        break;
    }
  }

  EXPECT_EQ(2u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);

  LexerResult result = mLexer.Lex(mIterator, mCountResumes, lexerFunc);
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, OneByteChunks)
{
  // Test delivering in one byte chunks.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    LexerResult result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i == 8) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  EXPECT_EQ(8u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, OneByteChunksWithUnbuffered)
{
  Vector<char> unbufferedVector;

  // Test delivering in one byte chunks.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    LexerResult result =
      mLexer.Lex(mIterator, mCountResumes,
                 [&](TestState aState, const char* aData, size_t aLength) {
        return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
    });

    if (i == 8) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  EXPECT_EQ(8u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, OneByteChunksWithYield)
{
  // Test delivering in one byte chunks.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    LexerResult result = mLexer.Lex(mIterator, mCountResumes, DoLexWithYield);

    switch (i) {
      case 2:
        EXPECT_TRUE(result.is<Yield>());
        EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());

        result = mLexer.Lex(mIterator, mCountResumes, DoLexWithYield);
        EXPECT_TRUE(result.is<Yield>());
        EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
        break;

      case 8:
        EXPECT_TRUE(result.is<TerminalState>());
        EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
        break;

      default:
        EXPECT_TRUE(i < 9);
        EXPECT_TRUE(result.is<Yield>());
        EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  EXPECT_EQ(8u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, ZeroLengthState)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Create a special StreamingLexer for this test because we want the first
  // state to be zero length.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 0),
                                  Transition::TerminateFailure());

  LexerResult result =
    lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthStates);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, ZeroLengthStatesAtEnd)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Create a special StreamingLexer for this test because we want the first
  // state to consume the full input.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 9),
                                  Transition::TerminateFailure());

  LexerResult result =
    lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthStatesAtEnd);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, ZeroLengthStateWithYield)
{
  // Create a special StreamingLexer for this test because we want the first
  // state to be zero length.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 0),
                                  Transition::TerminateFailure());

  mSourceBuffer->Append(mData, 3);
  LexerResult result =
    lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthYield);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());

  result = lexer.Lex(mIterator, mCountResumes, DoLexWithZeroLengthYield);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());

  mSourceBuffer->Append(mData + 3, sizeof(mData) - 3);
  mSourceBuffer->Complete(NS_OK);
  result = lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthYield);
  ASSERT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
  EXPECT_EQ(1u, mCountResumes->Count());
}

TEST_F(ImageStreamingLexer, ZeroLengthStateWithUnbuffered)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Create a special StreamingLexer for this test because we want the first
  // state to be both zero length and unbuffered.
  StreamingLexer<TestState> lexer(Transition::ToUnbuffered(TestState::ONE,
                                                           TestState::UNBUFFERED,
                                                           0),
                                  Transition::TerminateFailure());

  LexerResult result =
    lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthStatesUnbuffered);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, ZeroLengthStateAfterUnbuffered)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Create a special StreamingLexer for this test because we want the first
  // state to be zero length.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 0),
                                  Transition::TerminateFailure());

  LexerResult result =
    lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthStatesAfterUnbuffered);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, ZeroLengthStateWithUnbufferedYield)
{
  size_t unbufferedCallCount = 0;
  auto lexerFunc = [&](TestState aState, const char* aData, size_t aLength)
                     -> LexerTransition<TestState> {
    switch (aState) {
      case TestState::ONE:
        EXPECT_EQ(0u, aLength);
        return Transition::TerminateSuccess();

      case TestState::UNBUFFERED:
        switch (unbufferedCallCount) {
          case 0:
            CheckLexedData(aData, aLength, 0, 3);
            unbufferedCallCount++;

            // Continue after yield, telling StreamingLexer we consumed 0 bytes.
            return Transition::ContinueUnbufferedAfterYield(TestState::UNBUFFERED, 0);

          case 1:
            CheckLexedData(aData, aLength, 0, 3);
            unbufferedCallCount++;

            // Continue after yield, telling StreamingLexer we consumed 2 bytes.
            return Transition::ContinueUnbufferedAfterYield(TestState::UNBUFFERED, 2);

          case 2:
            EXPECT_EQ(1u, aLength);
            CheckLexedData(aData, aLength, 2, 1);
            unbufferedCallCount++;

            // Continue after yield, telling StreamingLexer we consumed 1 bytes.
            return Transition::ContinueUnbufferedAfterYield(TestState::UNBUFFERED, 1);

          case 3:
            CheckLexedData(aData, aLength, 3, 6);
            unbufferedCallCount++;

            // Continue after yield, telling StreamingLexer we consumed 6 bytes.
            // We should transition to TestState::ONE when we return from the
            // yield.
            return Transition::ContinueUnbufferedAfterYield(TestState::UNBUFFERED, 6);
        }

        ADD_FAILURE() << "Too many invocations of TestState::UNBUFFERED";
        return Transition::TerminateFailure();

      default:
        MOZ_CRASH("Unexpected or unhandled TestState");
    }
  };

  // Create a special StreamingLexer for this test because we want the first
  // state to be unbuffered.
  StreamingLexer<TestState> lexer(Transition::ToUnbuffered(TestState::ONE,
                                                           TestState::UNBUFFERED,
                                                           sizeof(mData)),
                                  Transition::TerminateFailure());

  mSourceBuffer->Append(mData, 3);
  LexerResult result = lexer.Lex(mIterator, mExpectNoResume, lexerFunc);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());
  EXPECT_EQ(1u, unbufferedCallCount);

  result = lexer.Lex(mIterator, mExpectNoResume, lexerFunc);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());
  EXPECT_EQ(2u, unbufferedCallCount);

  result = lexer.Lex(mIterator, mExpectNoResume, lexerFunc);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());
  EXPECT_EQ(3u, unbufferedCallCount);

  result = lexer.Lex(mIterator, mCountResumes, lexerFunc);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
  EXPECT_EQ(3u, unbufferedCallCount);

  mSourceBuffer->Append(mData + 3, 6);
  mSourceBuffer->Complete(NS_OK);
  EXPECT_EQ(1u, mCountResumes->Count());
  result = lexer.Lex(mIterator, mExpectNoResume, lexerFunc);
  ASSERT_TRUE(result.is<Yield>());
  EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());
  EXPECT_EQ(4u, unbufferedCallCount);

  result = lexer.Lex(mIterator, mExpectNoResume, lexerFunc);
  ASSERT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, TerminateSuccess)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Test that Terminate is "sticky".
  SourceBufferIterator iterator = mSourceBuffer->Iterator();
  LexerResult result =
    mLexer.Lex(iterator, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateSuccess();
  });
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());

  SourceBufferIterator iterator2 = mSourceBuffer->Iterator();
  result =
    mLexer.Lex(iterator2, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, TerminateFailure)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Test that Terminate is "sticky".
  SourceBufferIterator iterator = mSourceBuffer->Iterator();
  LexerResult result =
    mLexer.Lex(iterator, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::FAILURE, result.as<TerminalState>());

  SourceBufferIterator iterator2 = mSourceBuffer->Iterator();
  result =
    mLexer.Lex(iterator2, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::FAILURE, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, TerminateUnbuffered)
{
  // Test that Terminate works during an unbuffered read.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    LexerResult result =
      mLexer.Lex(mIterator, mCountResumes, DoLexWithUnbufferedTerminate);

    if (i > 2) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  // We expect 3 resumes because TestState::ONE consumes 3 bytes and then
  // transitions to TestState::UNBUFFERED, which calls TerminateSuccess() as
  // soon as it receives a single byte. That's four bytes total,  which are
  // delivered one at a time, requiring 3 resumes.
  EXPECT_EQ(3u, mCountResumes->Count());

  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, TerminateAfterYield)
{
  // Test that Terminate works after yielding.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    LexerResult result =
      mLexer.Lex(mIterator, mCountResumes, DoLexWithTerminateAfterYield);

    if (i > 2) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else if (i == 2) {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::OUTPUT_AVAILABLE, result.as<Yield>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  // We expect 2 resumes because TestState::ONE consumes 3 bytes and then
  // yields. When the lexer resumes at TestState::TWO, which receives the same 3
  // bytes, TerminateSuccess() gets called immediately.  That's three bytes
  // total, which are delivered one at a time, requiring 2 resumes.
  EXPECT_EQ(2u, mCountResumes->Count());

  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, SourceBufferImmediateComplete)
{
  // Test calling SourceBuffer::Complete() without appending any data. This
  // causes the SourceBuffer to automatically have a failing completion status,
  // no matter what you pass, so we expect TerminalState::FAILURE below.
  mSourceBuffer->Complete(NS_OK);

  LexerResult result = mLexer.Lex(mIterator, mExpectNoResume, DoLex);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::FAILURE, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, SourceBufferTruncatedTerminalStateSuccess)
{
  // Test that using a terminal state (in this case TerminalState::SUCCESS) as a
  // truncated state works.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3),
                                  Transition::TerminateSuccess());

  CheckTruncatedState(lexer, TerminalState::SUCCESS);
}

TEST_F(ImageStreamingLexer, SourceBufferTruncatedTerminalStateFailure)
{
  // Test that using a terminal state (in this case TerminalState::FAILURE) as a
  // truncated state works.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3),
                                  Transition::TerminateFailure());

  CheckTruncatedState(lexer, TerminalState::FAILURE);
}

TEST_F(ImageStreamingLexer, SourceBufferTruncatedStateReturningSuccess)
{
  // Test that a truncated state that returns TerminalState::SUCCESS works. When
  // |lexer| discovers that the data is truncated, it invokes the
  // TRUNCATED_SUCCESS state, which returns TerminalState::SUCCESS.
  // CheckTruncatedState() verifies that this happens.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3),
                                  Transition::To(TestState::TRUNCATED_SUCCESS, 0));

  CheckTruncatedState(lexer, TerminalState::SUCCESS);
}

TEST_F(ImageStreamingLexer, SourceBufferTruncatedStateReturningFailure)
{
  // Test that a truncated state that returns TerminalState::FAILURE works. When
  // |lexer| discovers that the data is truncated, it invokes the
  // TRUNCATED_FAILURE state, which returns TerminalState::FAILURE.
  // CheckTruncatedState() verifies that this happens.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3),
                                  Transition::To(TestState::TRUNCATED_FAILURE, 0));

  CheckTruncatedState(lexer, TerminalState::FAILURE);
}

TEST_F(ImageStreamingLexer, SourceBufferTruncatedFailingCompleteStatus)
{
  // Test that calling SourceBuffer::Complete() with a failing status results in
  // an immediate TerminalState::FAILURE result. (Note that |lexer|'s truncated
  // state is TerminalState::SUCCESS, so if we ignore the failing status, the
  // test will fail.)
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3),
                                  Transition::TerminateSuccess());

  CheckTruncatedState(lexer, TerminalState::FAILURE, NS_ERROR_FAILURE);
}

TEST_F(ImageStreamingLexer, NoSourceBufferResumable)
{
  // Test delivering in one byte chunks with no IResumable.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    LexerResult result = mLexer.Lex(mIterator, nullptr, DoLex);

    if (i == 8) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  mSourceBuffer->Complete(NS_OK);
}
