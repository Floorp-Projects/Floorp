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
CheckData(const char* aData, size_t aLength)
{
  EXPECT_TRUE(aLength == 3);
  EXPECT_EQ(1, aData[0]);
  EXPECT_EQ(2, aData[1]);
  EXPECT_EQ(3, aData[2]);
}

LexerTransition<TestState>
DoLex(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      CheckData(aData, aLength);
      return Transition::To(TestState::TWO, 3);
    case TestState::TWO:
      CheckData(aData, aLength);
      return Transition::To(TestState::THREE, 3);
    case TestState::THREE:
      CheckData(aData, aLength);
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unknown TestState");
  }
}

LexerTransition<TestState>
DoLexWithUnbuffered(TestState aState, const char* aData, size_t aLength,
                    Vector<char>& aUnbufferedVector)
{
  switch (aState) {
    case TestState::ONE:
      CheckData(aData, aLength);
      return Transition::ToUnbuffered(TestState::TWO, TestState::UNBUFFERED, 3);
    case TestState::UNBUFFERED:
      EXPECT_TRUE(aLength <= 3);
      EXPECT_TRUE(aUnbufferedVector.append(aData, aLength));
      return Transition::ContinueUnbuffered(TestState::UNBUFFERED);
    case TestState::TWO:
      CheckData(aUnbufferedVector.begin(), aUnbufferedVector.length());
      return Transition::To(TestState::THREE, 3);
    case TestState::THREE:
      CheckData(aData, aLength);
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unknown TestState");
  }
}

LexerTransition<TestState>
DoLexWithUnbufferedTerminate(TestState aState, const char* aData, size_t aLength)
{
  switch (aState) {
    case TestState::ONE:
      CheckData(aData, aLength);
      return Transition::ToUnbuffered(TestState::TWO, TestState::UNBUFFERED, 3);
    case TestState::UNBUFFERED:
      return Transition::TerminateSuccess();
    default:
      MOZ_CRASH("Unknown TestState");
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
  const char mData[9] { 1, 2, 3, 1, 2, 3, 1, 2, 3 };
  StreamingLexer<TestState> mLexer;
  RefPtr<SourceBuffer> mSourceBuffer;
  SourceBufferIterator mIterator;
  RefPtr<ExpectNoResume> mExpectNoResume;
  RefPtr<CountResumes> mCountResumes;
};

TEST_F(ImageStreamingLexer, ZeroLengthData)
{
  // Test delivering a zero-length piece of data.
  Maybe<TerminalState> result = mLexer.Lex(mData, 0, DoLex);
  EXPECT_TRUE(result.isNothing());
}

TEST_F(ImageStreamingLexer, SingleChunk)
{
  // Test delivering all the data at once.
  Maybe<TerminalState> result = mLexer.Lex(mData, sizeof(mData), DoLex);
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST_F(ImageStreamingLexer, SingleChunkFromSourceBuffer)
{
  // Test delivering all the data at once.
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  Maybe<TerminalState> result = mLexer.Lex(mIterator, mExpectNoResume, DoLex);

  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST_F(ImageStreamingLexer, SingleChunkWithUnbuffered)
{
  Vector<char> unbufferedVector;

  // Test delivering all the data at once.
  Maybe<TerminalState> result =
    mLexer.Lex(mData, sizeof(mData),
               [&](TestState aState, const char* aData, size_t aLength) {
      return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST_F(ImageStreamingLexer, SingleChunkWithUnbufferedFromSourceBuffer)
{
  Vector<char> unbufferedVector;

  // Test delivering all the data at once.
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  Maybe<TerminalState> result =
    mLexer.Lex(mIterator, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
  });

  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST_F(ImageStreamingLexer, ChunkPerState)
{
  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0; i < 3; ++i) {
    Maybe<TerminalState> result = mLexer.Lex(mData + 3 * i, 3, DoLex);

    if (i == 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}

TEST_F(ImageStreamingLexer, ChunkPerStateFromSourceBuffer)
{
  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0; i < 3; ++i) {
    mSourceBuffer->Append(mData + 3 * i, 3);
    Maybe<TerminalState> result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i == 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
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
    Maybe<TerminalState> result =
      mLexer.Lex(mData + 3 * i, 3,
                 [&](TestState aState, const char* aData, size_t aLength) {
        return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
    });

    if (i == 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}

TEST_F(ImageStreamingLexer, ChunkPerStateWithUnbufferedFromSourceBuffer)
{
  Vector<char> unbufferedVector;

  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0; i < 3; ++i) {
    mSourceBuffer->Append(mData + 3 * i, 3);
    Maybe<TerminalState> result =
      mLexer.Lex(mIterator, mCountResumes,
                 [&](TestState aState, const char* aData, size_t aLength) {
        return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
    });

    if (i == 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }

  EXPECT_EQ(2u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, OneByteChunks)
{
  // Test delivering in one byte chunks.
  for (unsigned i = 0; i < 9; ++i) {
    Maybe<TerminalState> result = mLexer.Lex(mData + i, 1, DoLex);

    if (i == 8) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}

TEST_F(ImageStreamingLexer, OneByteChunksFromSourceBuffer)
{
  // Test delivering in one byte chunks.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    Maybe<TerminalState> result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i == 8) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
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
    Maybe<TerminalState> result =
      mLexer.Lex(mData + i, 1,
                 [&](TestState aState, const char* aData, size_t aLength) {
        return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
    });

    if (i == 8) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}

TEST_F(ImageStreamingLexer, OneByteChunksWithUnbufferedFromSourceBuffer)
{
  Vector<char> unbufferedVector;

  // Test delivering in one byte chunks.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    Maybe<TerminalState> result =
      mLexer.Lex(mIterator, mCountResumes,
                 [&](TestState aState, const char* aData, size_t aLength) {
        return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
    });

    if (i == 8) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }

  EXPECT_EQ(8u, mCountResumes->Count());
  mSourceBuffer->Complete(NS_OK);
}

TEST_F(ImageStreamingLexer, TerminateSuccess)
{
  // Test that Terminate is "sticky".
  Maybe<TerminalState> result =
    mLexer.Lex(mData, sizeof(mData),
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateSuccess();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);

  result =
    mLexer.Lex(mData, sizeof(mData),
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST_F(ImageStreamingLexer, TerminateSuccessFromSourceBuffer)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Test that Terminate is "sticky".
  SourceBufferIterator iterator = mSourceBuffer->Iterator();
  Maybe<TerminalState> result =
    mLexer.Lex(iterator, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateSuccess();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);

  SourceBufferIterator iterator2 = mSourceBuffer->Iterator();
  result =
    mLexer.Lex(iterator2, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST_F(ImageStreamingLexer, TerminateFailure)
{
  // Test that Terminate is "sticky".
  Maybe<TerminalState> result =
    mLexer.Lex(mData, sizeof(mData),
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::FAILURE), result);

  result =
    mLexer.Lex(mData, sizeof(mData),
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::FAILURE), result);
}

TEST_F(ImageStreamingLexer, TerminateFailureFromSourceBuffer)
{
  mSourceBuffer->Append(mData, sizeof(mData));
  mSourceBuffer->Complete(NS_OK);

  // Test that Terminate is "sticky".
  SourceBufferIterator iterator = mSourceBuffer->Iterator();
  Maybe<TerminalState> result =
    mLexer.Lex(iterator, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::FAILURE), result);

  SourceBufferIterator iterator2 = mSourceBuffer->Iterator();
  result =
    mLexer.Lex(iterator2, mExpectNoResume,
               [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::FAILURE), result);
}

TEST_F(ImageStreamingLexer, TerminateUnbuffered)
{
  // Test that Terminate works during an unbuffered read.
  for (unsigned i = 0; i < 9; ++i) {
    Maybe<TerminalState> result =
      mLexer.Lex(mData + i, 1, DoLexWithUnbufferedTerminate);

    if (i > 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}

TEST_F(ImageStreamingLexer, TerminateUnbufferedFromSourceBuffer)
{
  // Test that Terminate works during an unbuffered read.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    Maybe<TerminalState> result =
      mLexer.Lex(mIterator, mCountResumes, DoLexWithUnbufferedTerminate);

    if (i > 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
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

  Maybe<TerminalState> result = mLexer.Lex(mIterator, mExpectNoResume, DoLex);

  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::FAILURE), result);
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

    Maybe<TerminalState> result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i >= 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
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

    Maybe<TerminalState> result = mLexer.Lex(mIterator, mCountResumes, DoLex);

    if (i >= 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::FAILURE), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }

  EXPECT_EQ(2u, mCountResumes->Count());
}

TEST_F(ImageStreamingLexer, NoSourceBufferResumable)
{
  // Test delivering in one byte chunks with no IResumable.
  for (unsigned i = 0; i < 9; ++i) {
    mSourceBuffer->Append(mData + i, 1);
    Maybe<TerminalState> result = mLexer.Lex(mIterator, nullptr, DoLex);

    if (i == 8) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }

  mSourceBuffer->Complete(NS_OK);
}
