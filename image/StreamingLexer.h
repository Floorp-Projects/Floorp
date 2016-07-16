/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * StreamingLexer is a lexing framework designed to make it simple to write
 * image decoders without worrying about the details of how the data is arriving
 * from the network.
 */

#ifndef mozilla_image_StreamingLexer_h
#define mozilla_image_StreamingLexer_h

#include <algorithm>
#include <cstdint>
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"

namespace mozilla {
namespace image {

/// Buffering behaviors for StreamingLexer transitions.
enum class BufferingStrategy
{
  BUFFERED,   // Data will be buffered and processed in one chunk.
  UNBUFFERED  // Data will be processed as it arrives, in multiple chunks.
};

/// Possible terminal states for the lexer.
enum class TerminalState
{
  SUCCESS,
  FAILURE
};

/// Possible yield reasons for the lexer.
enum class Yield
{
  NEED_MORE_DATA  // The lexer cannot continue without more data.
};

/// The result of a call to StreamingLexer::Lex().
typedef Variant<TerminalState, Yield> LexerResult;

/**
 * LexerTransition is a type used to give commands to the lexing framework.
 * Code that uses StreamingLexer can create LexerTransition values using the
 * static methods on Transition, and then return them to the lexing framework
 * for execution.
 */
template <typename State>
class LexerTransition
{
public:
  // This is implicit so that Terminate{Success,Failure}() can return a
  // TerminalState and have it implicitly converted to a
  // LexerTransition<State>, which avoids the need for a "<State>"
  // qualification to the Terminate{Success,Failure}() callsite.
  MOZ_IMPLICIT LexerTransition(TerminalState aFinalState)
    : mNextState(aFinalState)
  {}

  bool NextStateIsTerminal() const
  {
    return mNextState.template is<TerminalState>();
  }

  TerminalState NextStateAsTerminal() const
  {
    return mNextState.template as<TerminalState>();
  }

  State NextState() const
  {
    return mNextState.template as<NonTerminalState>().mState;
  }

  State UnbufferedState() const
  {
    return *mNextState.template as<NonTerminalState>().mUnbufferedState;
  }

  size_t Size() const
  {
    return mNextState.template as<NonTerminalState>().mSize;
  }

  BufferingStrategy Buffering() const
  {
    return mNextState.template as<NonTerminalState>().mBufferingStrategy;
  }

private:
  friend struct Transition;

  LexerTransition(State aNextState,
                  const Maybe<State>& aUnbufferedState,
                  size_t aSize,
                  BufferingStrategy aBufferingStrategy)
    : mNextState(NonTerminalState(aNextState, aUnbufferedState, aSize,
                                  aBufferingStrategy))
  {}

  struct NonTerminalState
  {
    State mState;
    Maybe<State> mUnbufferedState;
    size_t mSize;
    BufferingStrategy mBufferingStrategy;

    NonTerminalState(State aState,
                     const Maybe<State>& aUnbufferedState,
                     size_t aSize,
                     BufferingStrategy aBufferingStrategy)
      : mState(aState)
      , mUnbufferedState(aUnbufferedState)
      , mSize(aSize)
      , mBufferingStrategy(aBufferingStrategy)
    {
      MOZ_ASSERT_IF(mBufferingStrategy == BufferingStrategy::UNBUFFERED,
                    mUnbufferedState);
      MOZ_ASSERT_IF(mUnbufferedState,
                    mBufferingStrategy == BufferingStrategy::UNBUFFERED);
    }
  };
  Variant<NonTerminalState, TerminalState> mNextState;
};

struct Transition
{
  /// Transition to @aNextState, buffering @aSize bytes of data.
  template <typename State>
  static LexerTransition<State>
  To(const State& aNextState, size_t aSize)
  {
    return LexerTransition<State>(aNextState, Nothing(), aSize,
                                  BufferingStrategy::BUFFERED);
  }

  /**
   * Transition to @aNextState via @aUnbufferedState, reading @aSize bytes of
   * data unbuffered.
   *
   * The unbuffered data will be delivered in state @aUnbufferedState, which may
   * be invoked repeatedly until all @aSize bytes have been delivered. Then,
   * @aNextState will be invoked with no data. No state transitions are allowed
   * from @aUnbufferedState except for transitions to a terminal state, so
   * @aNextState will always be reached unless lexing terminates early.
   */
  template <typename State>
  static LexerTransition<State>
  ToUnbuffered(const State& aNextState,
               const State& aUnbufferedState,
               size_t aSize)
  {
    return LexerTransition<State>(aNextState, Some(aUnbufferedState), aSize,
                                  BufferingStrategy::UNBUFFERED);
  }

  /**
   * Continue receiving unbuffered data. @aUnbufferedState should be the same
   * state as the @aUnbufferedState specified in the preceding call to
   * ToUnbuffered().
   *
   * This should be used during an unbuffered read initiated by ToUnbuffered().
   */
  template <typename State>
  static LexerTransition<State>
  ContinueUnbuffered(const State& aUnbufferedState)
  {
    return LexerTransition<State>(aUnbufferedState, Nothing(), 0,
                                  BufferingStrategy::BUFFERED);
  }

  /**
   * Terminate lexing, ending up in terminal state SUCCESS. (The implicit
   * LexerTransition constructor will convert the result to a LexerTransition
   * as needed.)
   *
   * No more data will be delivered after this function is used.
   */
  static TerminalState
  TerminateSuccess()
  {
    return TerminalState::SUCCESS;
  }

  /**
   * Terminate lexing, ending up in terminal state FAILURE. (The implicit
   * LexerTransition constructor will convert the result to a LexerTransition
   * as needed.)
   *
   * No more data will be delivered after this function is used.
   */
  static TerminalState
  TerminateFailure()
  {
    return TerminalState::FAILURE;
  }

private:
  Transition();
};

/**
 * StreamingLexer is a lexing framework designed to make it simple to write
 * image decoders without worrying about the details of how the data is arriving
 * from the network.
 *
 * To use StreamingLexer:
 *
 *  - Create a State type. This should be an |enum class| listing all of the
 *    states that you can be in while lexing the image format you're trying to
 *    read.
 *
 *  - Add an instance of StreamingLexer<State> to your decoder class. Initialize
 *    it with a Transition::To() the state that you want to start lexing in.
 *
 *  - In your decoder's DoDecode() method, call Lex(), passing in the input
 *    data and length that are passed to DoDecode(). You also need to pass
 *    a lambda which dispatches to lexing code for each state based on the State
 *    value that's passed in. The lambda generally should just continue a
 *    |switch| statement that calls different methods for each State value. Each
 *    method should return a LexerTransition<State>, which the lambda should
 *    return in turn.
 *
 *  - Write the methods that actually implement lexing for your image format.
 *    These methods should return either Transition::To(), to move on to another
 *    state, or Transition::Terminate{Success,Failure}(), if lexing has
 *    terminated in either success or failure. (There are also additional
 *    transitions for unbuffered reads; see below.)
 *
 * That's the basics. The StreamingLexer will track your position in the input
 * and buffer enough data so that your lexing methods can process everything in
 * one pass. Lex() returns Yield::NEED_MORE_DATA if more data is needed, in
 * which case you should just return from DoDecode(). If lexing reaches a
 * terminal state, Lex() returns TerminalState::SUCCESS or
 * TerminalState::FAILURE, and you can check which one to determine if lexing
 * succeeded or failed and do any necessary cleanup.
 *
 * There's one more wrinkle: some lexers may want to *avoid* buffering in some
 * cases, and just process the data as it comes in. This is useful if, for
 * example, you just want to skip over a large section of data; there's no point
 * in buffering data you're just going to ignore.
 *
 * You can begin an unbuffered read with Transition::ToUnbuffered(). This works
 * a little differently than Transition::To() in that you specify *two* states.
 * The @aUnbufferedState argument specifies a state that will be called
 * repeatedly with unbuffered data, as soon as it arrives. The implementation
 * for that state should return either a transition to a terminal state, or
 * Transition::ContinueUnbuffered(). Once the amount of data requested in the
 * original call to Transition::ToUnbuffered() has been delivered, Lex() will
 * transition to the @aNextState state specified via Transition::ToUnbuffered().
 * That state will be invoked with *no* data; it's just called to signal that
 * the unbuffered read is over.
 *
 * XXX(seth): We should be able to get of the |State| stuff totally once bug
 * 1198451 lands, since we can then just return a function representing the next
 * state directly.
 */
template <typename State, size_t InlineBufferSize = 16>
class StreamingLexer
{
public:
  explicit StreamingLexer(LexerTransition<State> aStartState)
    : mTransition(TerminalState::FAILURE)
  {
    SetTransition(aStartState);
  }

  template <typename Func>
  LexerResult Lex(SourceBufferIterator& aIterator,
                  IResumable* aOnResume,
                  Func aFunc)
  {
    if (mTransition.NextStateIsTerminal()) {
      // We've already reached a terminal state. We never deliver any more data
      // in this case; just return the terminal state again immediately.
      return LexerResult(mTransition.NextStateAsTerminal());
    }

    Maybe<LexerResult> result;
    do {
      MOZ_ASSERT_IF(mTransition.Buffering() == BufferingStrategy::UNBUFFERED,
                    mUnbufferedState);

      // Figure out how much we need to read.
      const size_t toRead = mTransition.Buffering() == BufferingStrategy::UNBUFFERED
                          ? mUnbufferedState->mBytesRemaining
                          : mTransition.Size() - mBuffer.length();

      // Attempt to advance the iterator by |toRead| bytes.
      switch (aIterator.AdvanceOrScheduleResume(toRead, aOnResume)) {
        case SourceBufferIterator::WAITING:
          // We can't continue because the rest of the data hasn't arrived from
          // the network yet. We don't have to do anything special; the
          // SourceBufferIterator will ensure that |aOnResume| gets called when
          // more data is available.
          result = Some(LexerResult(Yield::NEED_MORE_DATA));
          break;

        case SourceBufferIterator::COMPLETE:
          // Normally even if the data is truncated, we want decoding to
          // succeed so we can display whatever we got. However, if the
          // SourceBuffer was completed with a failing status, we want to fail.
          // This happens only in exceptional situations like SourceBuffer
          // itself encountering a failure due to OOM.
          result = SetTransition(NS_SUCCEEDED(aIterator.CompletionStatus())
                 ? Transition::TerminateSuccess()
                 : Transition::TerminateFailure());
          break;

        case SourceBufferIterator::READY:
          // Process the new data that became available.
          MOZ_ASSERT(aIterator.Data());

          result = mTransition.Buffering() == BufferingStrategy::UNBUFFERED
                 ? UnbufferedRead(aIterator, aFunc)
                 : BufferedRead(aIterator, aFunc);
          break;

        default:
          MOZ_ASSERT_UNREACHABLE("Unknown SourceBufferIterator state");
          result = SetTransition(Transition::TerminateFailure());
      }
    } while (!result);

    return *result;
  }

private:
  template <typename Func>
  Maybe<LexerResult> UnbufferedRead(SourceBufferIterator& aIterator, Func aFunc)
  {
    MOZ_ASSERT(mTransition.Buffering() == BufferingStrategy::UNBUFFERED);
    MOZ_ASSERT(mUnbufferedState);
    MOZ_ASSERT(mBuffer.empty(),
               "Buffered read at the same time as unbuffered read?");
    MOZ_ASSERT(aIterator.Length() <= mUnbufferedState->mBytesRemaining,
               "Read too much data during unbuffered read?");

    if (mUnbufferedState->mBytesRemaining > 0) {
      // Call aFunc with the unbuffered state to indicate that we're in the
      // middle of an unbuffered read. We enforce that any state transition
      // passed back to us is either a terminal state or takes us back to the
      // unbuffered state.
      LexerTransition<State> unbufferedTransition =
        aFunc(mTransition.UnbufferedState(), aIterator.Data(), aIterator.Length());

      // If we reached a terminal state, we're done.
      if (unbufferedTransition.NextStateIsTerminal()) {
        return SetTransition(unbufferedTransition);
      }

      // We're continuing the read. Update our position.
      MOZ_ASSERT(mTransition.UnbufferedState() ==
                   unbufferedTransition.NextState());
      mUnbufferedState->mBytesRemaining -=
        std::min(mUnbufferedState->mBytesRemaining, aIterator.Length());

      // If we haven't read everything yet, try to read more data.
      if (mUnbufferedState->mBytesRemaining != 0) {
        return Nothing();  // Keep processing.
      }
    }

    // We're done with the unbuffered read, so transition to the next state.
    return SetTransition(aFunc(mTransition.NextState(), nullptr, 0));
  }

  template <typename Func>
  Maybe<LexerResult> BufferedRead(SourceBufferIterator& aIterator, Func aFunc)
  {
    MOZ_ASSERT(mTransition.Buffering() == BufferingStrategy::BUFFERED);
    MOZ_ASSERT(!mUnbufferedState,
               "Buffered read at the same time as unbuffered read?");
    MOZ_ASSERT(mBuffer.length() < mTransition.Size() ||
               (mBuffer.length() == 0 && mTransition.Size() == 0),
               "Buffered more than we needed?");

    // If we have all the data, we don't actually need to buffer anything.
    if (mBuffer.empty() && aIterator.Length() == mTransition.Size()) {
      return SetTransition(aFunc(mTransition.NextState(),
                                 aIterator.Data(),
                                 aIterator.Length()));
    }

    // We do need to buffer, so make sure the buffer has enough capacity. We
    // deliberately wait until we know for sure we need to buffer to call
    // reserve() since it could require memory allocation.
    if (!mBuffer.reserve(mTransition.Size())) {
      return SetTransition(Transition::TerminateFailure());
    }

    // Append the new data we just got to the buffer.
    if (!mBuffer.append(aIterator.Data(), aIterator.Length())) {
      return SetTransition(Transition::TerminateFailure());
    }

    if (mBuffer.length() != mTransition.Size()) {
      return Nothing();  // Keep processing.
    }

    // We've buffered everything, so transition to the next state.
    return SetTransition(aFunc(mTransition.NextState(),
                               mBuffer.begin(),
                               mBuffer.length()));
  }

  Maybe<LexerResult> SetTransition(const LexerTransition<State>& aTransition)
  {
    mTransition = aTransition;

    // Get rid of anything left over from the previous state.
    mBuffer.clear();
    mUnbufferedState = Nothing();

    // If we reached a terminal state, let the caller know.
    if (mTransition.NextStateIsTerminal()) {
      return Some(LexerResult(mTransition.NextStateAsTerminal()));
    }

    // If we're entering an unbuffered state, record how long we'll stay in it.
    if (mTransition.Buffering() == BufferingStrategy::UNBUFFERED) {
      mUnbufferedState.emplace(mTransition.Size());
    }

    return Nothing();  // Keep processing.
  }

  // State that tracks our position within an unbuffered read.
  struct UnbufferedState
  {
    explicit UnbufferedState(size_t aBytesRemaining)
      : mBytesRemaining(aBytesRemaining)
    { }

    size_t mBytesRemaining;
  };

  Vector<char, InlineBufferSize> mBuffer;
  LexerTransition<State> mTransition;
  Maybe<UnbufferedState> mUnbufferedState;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_StreamingLexer_h
