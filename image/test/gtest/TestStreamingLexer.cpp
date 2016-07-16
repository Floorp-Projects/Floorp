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
  UNBUFFERED
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
    case TestState::UNBUFFERED:
      EXPECT_TRUE(aLength <= 3);
      EXPECT_TRUE(aUnbufferedVector.append(aData, aLength));
      return Transition::ContinueUnbuffered(TestState::UNBUFFERED);
    case TestState::TWO:
      CheckLexedData(aUnbufferedVector.begin(), aUnbufferedVector.length(), 3, 3);
      return Transition::To(TestState::THREE, 3);
    case TestState::THREE:
      CheckLexedData(aData, aLength, 6, 3);
      return Transition::TerminateSuccess();
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

class ImageStreamingLexer : public ::testing::Test
{
public:
  ImageStreamingLexer()
    : mLexer(Transition::To(TestState::ONE, 3))
    , mSourceBuffer(new SourceBuffer)
    , mIterator(mSourceBuffer->Iterator())
    , mExpectNoResume(new ExpectNoResume)
    , mCountResumes(new CountResumes)
  { }

protected:
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

TEST_F(ImageStreamingLexer, ZeroLengthState)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Create a special StreamingLexer for this test because we want the first
  // state to be zero length.
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 0));

  LexerResult result =
    lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthStates);

  EXPECT_TRUE(result.is<TerminalState>());
  EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
}

TEST_F(ImageStreamingLexer, ZeroLengthStateWithUnbuffered)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Create a special StreamingLexer for this test because we want the first
  // state to be both zero length and unbuffered.
  StreamingLexer<TestState> lexer(Transition::ToUnbuffered(TestState::ONE,
                                                           TestState::UNBUFFERED,
                                                           0));

  LexerResult result =
    lexer.Lex(mIterator, mExpectNoResume, DoLexWithZeroLengthStatesUnbuffered);

  EXPECT_TRUE(result.is<TerminalState>());
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

TEST_F(ImageStreamingLexer, SourceBufferTruncatedSuccess)
{
  // Test that calling SourceBuffer::Complete() with a successful status results
  // in an immediate TerminalState::SUCCESS result.
  for (unsigned i = 0; i < 9; ++i) {
    if (i < 2) {
      mSourceBuffer->Append(mData + i, 1);
    } else if (i == 2) {
      mSourceBuffer->Complete(NS_OK);
    }

    LexerResult result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i >= 2) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::SUCCESS, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  EXPECT_EQ(2u, mCountResumes->Count());
}

TEST_F(ImageStreamingLexer, SourceBufferTruncatedFailure)
{
  // Test that calling SourceBuffer::Complete() with a failing status results in
  // an immediate TerminalState::FAILURE result.
  for (unsigned i = 0; i < 9; ++i) {
    if (i < 2) {
      mSourceBuffer->Append(mData + i, 1);
    } else if (i == 2) {
      mSourceBuffer->Complete(NS_ERROR_FAILURE);
    }

    LexerResult result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i >= 2) {
      EXPECT_TRUE(result.is<TerminalState>());
      EXPECT_EQ(TerminalState::FAILURE, result.as<TerminalState>());
    } else {
      EXPECT_TRUE(result.is<Yield>());
      EXPECT_EQ(Yield::NEED_MORE_DATA, result.as<Yield>());
    }
  }

  EXPECT_EQ(2u, mCountResumes->Count());
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
