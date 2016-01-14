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

TEST(ImageStreamingLexer, SingleChunk)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };

  // Test delivering all the data at once.
  Maybe<TerminalState> result = lexer.Lex(data, sizeof(data), DoLex);
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST(ImageStreamingLexer, SingleChunkWithUnbuffered)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };
  Vector<char> unbufferedVector;

  // Test delivering all the data at once.
  Maybe<TerminalState> result =
    lexer.Lex(data, sizeof(data),
              [&](TestState aState, const char* aData, size_t aLength) {
      return DoLexWithUnbuffered(aState, aData, aLength, unbufferedVector);
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST(ImageStreamingLexer, ChunkPerState)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };

  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0 ; i < 3 ; ++i) {
    Maybe<TerminalState> result = lexer.Lex(data + 3 * i, 3, DoLex);

    if (i == 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}

TEST(ImageStreamingLexer, ChunkPerStateWithUnbuffered)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };
  Vector<char> unbufferedVector;

  // Test delivering in perfectly-sized chunks, one per state.
  for (unsigned i = 0 ; i < 3 ; ++i) {
    Maybe<TerminalState> result =
      lexer.Lex(data + 3 * i, 3,
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

TEST(ImageStreamingLexer, OneByteChunks)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };

  // Test delivering in one byte chunks.
  for (unsigned i = 0 ; i < 9 ; ++i) {
    Maybe<TerminalState> result = lexer.Lex(data + i, 1, DoLex);

    if (i == 8) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}

TEST(ImageStreamingLexer, OneByteChunksWithUnbuffered)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };
  Vector<char> unbufferedVector;

  // Test delivering in one byte chunks.
  for (unsigned i = 0 ; i < 9 ; ++i) {
    Maybe<TerminalState> result =
      lexer.Lex(data + i, 1,
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

TEST(ImageStreamingLexer, TerminateSuccess)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };

  // Test that Terminate is "sticky".
  Maybe<TerminalState> result =
    lexer.Lex(data, sizeof(data),
              [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateSuccess();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);

  result =
    lexer.Lex(data, sizeof(data),
              [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::SUCCESS), result);
}

TEST(ImageStreamingLexer, TerminateFailure)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };

  // Test that Terminate is "sticky".
  Maybe<TerminalState> result =
    lexer.Lex(data, sizeof(data),
              [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(aState == TestState::ONE);
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::FAILURE), result);

  result =
    lexer.Lex(data, sizeof(data),
              [&](TestState aState, const char* aData, size_t aLength) {
      EXPECT_TRUE(false);  // Shouldn't get here.
      return Transition::TerminateFailure();
  });
  EXPECT_TRUE(result.isSome());
  EXPECT_EQ(Some(TerminalState::FAILURE), result);
}

TEST(ImageStreamingLexer, TerminateUnbuffered)
{
  StreamingLexer<TestState> lexer(Transition::To(TestState::ONE, 3));
  char data[9] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };

  // Test that Terminate works during an unbuffered read.
  for (unsigned i = 0 ; i < 9 ; ++i) {
    Maybe<TerminalState> result =
      lexer.Lex(data + i, 1, DoLexWithUnbufferedTerminate);

    if (i > 2) {
      EXPECT_TRUE(result.isSome());
      EXPECT_EQ(Some(TerminalState::SUCCESS), result);
    } else {
      EXPECT_TRUE(result.isNothing());
    }
  }
}
