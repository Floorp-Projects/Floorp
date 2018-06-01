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
#include "mozilla/Move.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "SourceBuffer.h"

namespace mozilla {
namespace image {

/// Buffering behaviors for StreamingLexer transitions.
enum class BufferingStrategy
{
  BUFFERED,   // Data will be buffered and processed in one chunk.
  UNBUFFERED  // Data will be processed as it arrives, in multiple chunks.
};

/// Control flow behaviors for StreamingLexer transitions.
enum class ControlFlowStrategy
{
  CONTINUE,  // If there's enough data, proceed to the next state immediately.
  YIELD      // Yield to the caller before proceeding to the next state.
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
  NEED_MORE_DATA,   // The lexer cannot continue without more data.
  OUTPUT_AVAILABLE  // There is output available for the caller to consume.
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

  ControlFlowStrategy ControlFlow() const
  {
    return mNextState.template as<NonTerminalState>().mControlFlowStrategy;
  }

private:
  friend struct Transition;

  LexerTransition(State aNextState,
                  const Maybe<State>& aUnbufferedState,
                  size_t aSize,
                  BufferingStrategy aBufferingStrategy,
                  ControlFlowStrategy aControlFlowStrategy)
    : mNextState(NonTerminalState(aNextState, aUnbufferedState, aSize,
                                  aBufferingStrategy, aControlFlowStrategy))
  {}

  struct NonTerminalState
  {
    State mState;
    Maybe<State> mUnbufferedState;
    size_t mSize;
    BufferingStrategy mBufferingStrategy;
    ControlFlowStrategy mControlFlowStrategy;

    NonTerminalState(State aState,
                     const Maybe<State>& aUnbufferedState,
                     size_t aSize,
                     BufferingStrategy aBufferingStrategy,
                     ControlFlowStrategy aControlFlowStrategy)
      : mState(aState)
      , mUnbufferedState(aUnbufferedState)
      , mSize(aSize)
      , mBufferingStrategy(aBufferingStrategy)
      , mControlFlowStrategy(aControlFlowStrategy)
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
                                  BufferingStrategy::BUFFERED,
                                  ControlFlowStrategy::CONTINUE);
  }

  /// Yield to the caller, transitioning to @aNextState when Lex() is next
  /// invoked. The same data that was delivered for the current state will be
  /// delivered again.
  template <typename State>
  static LexerTransition<State>
  ToAfterYield(const State& aNextState)
  {
    return LexerTransition<State>(aNextState, Nothing(), 0,
                                  BufferingStrategy::BUFFERED,
                                  ControlFlowStrategy::YIELD);
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
                                  BufferingStrategy::UNBUFFERED,
                                  ControlFlowStrategy::CONTINUE);
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
                                  BufferingStrategy::BUFFERED,
                                  ControlFlowStrategy::CONTINUE);
  }

  /**
   * Continue receiving unbuffered data. @aUnbufferedState should be the same
   * state as the @aUnbufferedState specified in the preceding call to
   * ToUnbuffered(). @aSize indicates the amount of data that has already been
   * consumed; the next state will receive the same data that was delivered to
   * the current state, without the first @aSize bytes.
   *
   * This should be used during an unbuffered read initiated by ToUnbuffered().
   */
  template <typename State>
  static LexerTransition<State>
  ContinueUnbufferedAfterYield(const State& aUnbufferedState, size_t aSize)
  {
    return LexerTransition<State>(aUnbufferedState, Nothing(), aSize,
                                  BufferingStrategy::BUFFERED,
                                  ControlFlowStrategy::YIELD);
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
 *    it with a Transition::To() the state that you want to start lexing in, and
 *    a Transition::To() the state you'd like to use to handle truncated data.
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
 * Sometimes, the input data is truncated. StreamingLexer will notify you when
 * this happens by invoking the truncated data state you passed to the
 * constructor. At this point you can attempt to recover and return
 * TerminalState::SUCCESS or TerminalState::FAILURE, depending on whether you
 * were successful. Note that you can't return anything other than a terminal
 * state in this situation, since there's no more data to read. For the same
 * reason, your truncated data state shouldn't require any data. (That is, the
 * @aSize argument you pass to Transition::To() must be zero.) Violating these
 * requirements will trigger assertions and an immediate transition to
 * TerminalState::FAILURE.
 *
 * Some lexers may want to *avoid* buffering in some cases, and just process the
 * data as it comes in. This is useful if, for example, you just want to skip
 * over a large section of data; there's no point in buffering data you're just
 * going to ignore.
 *
 * You can begin an unbuffered read with Transition::ToUnbuffered(). This works
 * a little differently than Transition::To() in that you specify *two* states.
 * The @aUnbufferedState argument specifies a state that will be called
 * repeatedly with unbuffered data, as soon as it arrives. The implementation
 * for that state should return either a transition to a terminal state, or a
 * Transition::ContinueUnbuffered() to the same @aUnbufferedState. (From a
 * technical perspective, it's not necessary to specify the state again, but
 * it's helpful to human readers.) Once the amount of data requested in the
 * original call to Transition::ToUnbuffered() has been delivered, Lex() will
 * transition to the @aNextState state specified via Transition::ToUnbuffered().
 * That state will be invoked with *no* data; it's just called to signal that
 * the unbuffered read is over.
 *
 * It's sometimes useful for a lexer to provide incremental results, rather
 * than simply running to completion and presenting all its output at once. For
 * example, when decoding animated images, it may be useful to produce each
 * frame incrementally. StreamingLexer supports this by allowing a lexer to
 * yield.
 *
 * To yield back to the caller, a state implementation can simply return
 * Transition::ToAfterYield(). ToAfterYield()'s @aNextState argument specifies
 * the next state that the lexer should transition to, just like when using
 * Transition::To(), but there are two differences. One is that Lex() will
 * return to the caller before processing any more data when it encounters a
 * yield transition. This provides an opportunity for the caller to interact with the
 * lexer's intermediate results. The second difference is that @aNextState
 * will be called with *the same data as the state that you returned
 * Transition::ToAfterYield() from*. This allows a lexer to partially consume
 * the data, return intermediate results, and then finish consuming the data
 * when @aNextState is called.
 *
 * It's also possible to yield during an unbuffered read. Just return a
 * Transition::ContinueUnbufferedAfterYield(). Just like with
 * Transition::ContinueUnbuffered(), the @aUnbufferedState must be the same as
 * the one originally passed to Transition::ToUnbuffered(). The second argument,
 * @aSize, specifies the amount of data that the lexer has already consumed.
 * When @aUnbufferedState is next invoked, it will get the same data that it
 * received previously, except that the first @aSize bytes will be excluded.
 * This makes it easy to consume unbuffered data incrementally.
 *
 * XXX(seth): We should be able to get of the |State| stuff totally once bug
 * 1198451 lands, since we can then just return a function representing the next
 * state directly.
 */
template <typename State, size_t InlineBufferSize = 16>
class StreamingLexer
{
public:
  StreamingLexer(const LexerTransition<State>& aStartState,
                 const LexerTransition<State>& aTruncatedState)
    : mTransition(TerminalState::FAILURE)
    , mTruncatedTransition(aTruncatedState)
  {
    if (!aStartState.NextStateIsTerminal() &&
        aStartState.ControlFlow() == ControlFlowStrategy::YIELD) {
      // Allowing a StreamingLexer to start in a yield state doesn't make sense
      // semantically (since yield states are supposed to deliver the same data
      // as previous states, and there's no previous state here), but more
      // importantly, it's necessary to advance a SourceBufferIterator at least
      // once before you can read from it, and adding the necessary checks to
      // Lex() to avoid that issue has the potential to mask real bugs. So
      // instead, it's better to forbid starting in a yield state.
      MOZ_ASSERT_UNREACHABLE("Starting in a yield state");
      return;
    }

    if (!aTruncatedState.NextStateIsTerminal() &&
          (aTruncatedState.ControlFlow() == ControlFlowStrategy::YIELD ||
           aTruncatedState.Buffering() == BufferingStrategy::UNBUFFERED ||
           aTruncatedState.Size() != 0)) {
      // The truncated state can't receive any data because, by definition,
      // there is no more data to receive. That means that yielding or an
      // unbuffered read would not make sense, and that the state must require
      // zero bytes.
      MOZ_ASSERT_UNREACHABLE("Truncated state makes no sense");
      return;
    }

    SetTransition(aStartState);
  }

  /**
   * From the given SourceBufferIterator, aIterator, create a new iterator at
   * the same position, with the given read limit, aReadLimit. The read limit
   * applies after adjusting for the position. If the given iterator has been
   * advanced, but required buffering inside StreamingLexer, the position
   * of the cloned iterator will be at the beginning of buffered data; this
   * should match the perspective of the caller.
   */
  Maybe<SourceBufferIterator> Clone(SourceBufferIterator& aIterator,
                                    size_t aReadLimit) const
  {
    // In order to advance to the current position of the iterator from the
    // perspective of the caller, we need to take into account if we are
    // buffering data.
    size_t pos = aIterator.Position();
    if (!mBuffer.empty()) {
      pos += aIterator.Length();
      MOZ_ASSERT(pos > mBuffer.length());
      pos -= mBuffer.length();
    }

    size_t readLimit = aReadLimit;
    if (aReadLimit != SIZE_MAX) {
      readLimit += pos;
    }

    SourceBufferIterator other = aIterator.Owner()->Iterator(readLimit);

    // Since the current iterator has already advanced to this point, we
    // know that the state can only be READY or COMPLETE. That does not mean
    // everything is stored in a single chunk, and may require multiple Advance
    // calls to get where we want to be.
    SourceBufferIterator::State state;
    do {
      state = other.Advance(pos);
      if (state != SourceBufferIterator::READY) {
        // The only way we should fail to advance over data we already seen is
        // if we hit an error while inserting data into the buffer. This will
        // cause an early exit.
        MOZ_ASSERT(NS_FAILED(other.CompletionStatus()));
        return Nothing();
      }
      MOZ_ASSERT(pos >= other.Length());
      pos -= other.Length();
    } while (pos > 0);

    // Force the data pointer to be where we expect it to be.
    state = other.Advance(0);
    if (state != SourceBufferIterator::READY) {
      // The current position could be the end of the buffer, in which case
      // there is no point cloning with no more data to read.
      MOZ_ASSERT(state == SourceBufferIterator::COMPLETE);
      return Nothing();
    }
    return Some(std::move(other));
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

    // If the lexer requested a yield last time, we deliver the same data again
    // before we read anything else from |aIterator|. Note that although to the
    // callers of Lex(), Yield::NEED_MORE_DATA is just another type of yield,
    // internally they're different in that we don't redeliver the same data in
    // the Yield::NEED_MORE_DATA case, and |mYieldingToState| is not set. This
    // means that for Yield::NEED_MORE_DATA, we go directly to the loop below.
    if (mYieldingToState) {
      result = mTransition.Buffering() == BufferingStrategy::UNBUFFERED
             ? UnbufferedReadAfterYield(aIterator, aFunc)
             : BufferedReadAfterYield(aIterator, aFunc);
    }

    while (!result) {
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
          // The data is truncated; if not, the lexer would've reached a
          // terminal state by now. We only get to
          // SourceBufferIterator::COMPLETE after every byte of data has been
          // delivered to the lexer.
          result = Truncated(aIterator, aFunc);
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
    };

    return *result;
  }

private:
  template <typename Func>
  Maybe<LexerResult> UnbufferedRead(SourceBufferIterator& aIterator, Func aFunc)
  {
    MOZ_ASSERT(mTransition.Buffering() == BufferingStrategy::UNBUFFERED);
    MOZ_ASSERT(mUnbufferedState);
    MOZ_ASSERT(!mYieldingToState);
    MOZ_ASSERT(mBuffer.empty(),
               "Buffered read at the same time as unbuffered read?");
    MOZ_ASSERT(aIterator.Length() <= mUnbufferedState->mBytesRemaining,
               "Read too much data during unbuffered read?");
    MOZ_ASSERT(mUnbufferedState->mBytesConsumedInCurrentChunk == 0,
               "Already consumed data in the current chunk, but not yielding?");

    if (mUnbufferedState->mBytesRemaining == 0) {
      // We're done with the unbuffered read, so transition to the next state.
      return SetTransition(aFunc(mTransition.NextState(), nullptr, 0));
    }

    return ContinueUnbufferedRead(aIterator.Data(), aIterator.Length(),
                                  aIterator.Length(), aFunc);
  }

  template <typename Func>
  Maybe<LexerResult> UnbufferedReadAfterYield(SourceBufferIterator& aIterator, Func aFunc)
  {
    MOZ_ASSERT(mTransition.Buffering() == BufferingStrategy::UNBUFFERED);
    MOZ_ASSERT(mUnbufferedState);
    MOZ_ASSERT(mYieldingToState);
    MOZ_ASSERT(mBuffer.empty(),
               "Buffered read at the same time as unbuffered read?");
    MOZ_ASSERT(aIterator.Length() <= mUnbufferedState->mBytesRemaining,
               "Read too much data during unbuffered read?");
    MOZ_ASSERT(mUnbufferedState->mBytesConsumedInCurrentChunk <= aIterator.Length(),
               "Consumed more data than the current chunk holds?");
    MOZ_ASSERT(mTransition.UnbufferedState() == *mYieldingToState);

    mYieldingToState = Nothing();

    if (mUnbufferedState->mBytesRemaining == 0) {
      // We're done with the unbuffered read, so transition to the next state.
      return SetTransition(aFunc(mTransition.NextState(), nullptr, 0));
    }

    // Since we've yielded, we may have already consumed some data in this
    // chunk. Make the necessary adjustments. (Note that the std::min call is
    // just belt-and-suspenders to keep this code memory safe even if there's
    // a bug somewhere.)
    const size_t toSkip =
      std::min(mUnbufferedState->mBytesConsumedInCurrentChunk, aIterator.Length());
    const char* data = aIterator.Data() + toSkip;
    const size_t length = aIterator.Length() - toSkip;

    // If |length| is zero, we've hit the end of the current chunk. This only
    // happens if we yield right at the end of a chunk. Rather than call |aFunc|
    // with a |length| of zero bytes (which seems potentially surprising to
    // decoder authors), we go ahead and read more data.
    if (length == 0) {
      return FinishCurrentChunkOfUnbufferedRead(aIterator.Length());
    }

    return ContinueUnbufferedRead(data, length, aIterator.Length(), aFunc);
  }

  template <typename Func>
  Maybe<LexerResult> ContinueUnbufferedRead(const char* aData,
                                            size_t aLength,
                                            size_t aChunkLength,
                                            Func aFunc)
  {
    // Call aFunc with the unbuffered state to indicate that we're in the
    // middle of an unbuffered read. We enforce that any state transition
    // passed back to us is either a terminal state or takes us back to the
    // unbuffered state.
    LexerTransition<State> unbufferedTransition =
      aFunc(mTransition.UnbufferedState(), aData, aLength);

    // If we reached a terminal state, we're done.
    if (unbufferedTransition.NextStateIsTerminal()) {
      return SetTransition(unbufferedTransition);
    }

    MOZ_ASSERT(mTransition.UnbufferedState() ==
                 unbufferedTransition.NextState());

    // Perform bookkeeping.
    if (unbufferedTransition.ControlFlow() == ControlFlowStrategy::YIELD) {
      mUnbufferedState->mBytesConsumedInCurrentChunk += unbufferedTransition.Size();
      return SetTransition(unbufferedTransition);
    }

    MOZ_ASSERT(unbufferedTransition.Size() == 0);
    return FinishCurrentChunkOfUnbufferedRead(aChunkLength);
  }

  Maybe<LexerResult> FinishCurrentChunkOfUnbufferedRead(size_t aChunkLength)
  {
    // We've finished an unbuffered read of a chunk of length |aChunkLength|, so
    // update |myBytesRemaining| to reflect that we're |aChunkLength| closer to
    // the end of the unbuffered read. (The std::min call is just
    // belt-and-suspenders to keep this code memory safe even if there's a bug
    // somewhere.)
    mUnbufferedState->mBytesRemaining -=
      std::min(mUnbufferedState->mBytesRemaining, aChunkLength);

    // Since we're moving on to a new chunk, we can forget about the count of
    // bytes consumed by yielding in the current chunk.
    mUnbufferedState->mBytesConsumedInCurrentChunk = 0;

    return Nothing();  // Keep processing.
  }

  template <typename Func>
  Maybe<LexerResult> BufferedRead(SourceBufferIterator& aIterator, Func aFunc)
  {
    MOZ_ASSERT(mTransition.Buffering() == BufferingStrategy::BUFFERED);
    MOZ_ASSERT(!mYieldingToState);
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

  template <typename Func>
  Maybe<LexerResult> BufferedReadAfterYield(SourceBufferIterator& aIterator,
                                            Func aFunc)
  {
    MOZ_ASSERT(mTransition.Buffering() == BufferingStrategy::BUFFERED);
    MOZ_ASSERT(mYieldingToState);
    MOZ_ASSERT(!mUnbufferedState,
               "Buffered read at the same time as unbuffered read?");
    MOZ_ASSERT(mBuffer.length() <= mTransition.Size(),
               "Buffered more than we needed?");

    State nextState = std::move(*mYieldingToState);

    // After a yield, we need to take the same data that we delivered to the
    // last state, and deliver it again to the new state. We know that this is
    // happening right at a state transition, and that the last state was a
    // buffered read, so there are two cases:

    // 1. We got the data from the SourceBufferIterator directly.
    if (mBuffer.empty() && aIterator.Length() == mTransition.Size()) {
      return SetTransition(aFunc(nextState,
                                 aIterator.Data(),
                                 aIterator.Length()));
    }

    // 2. We got the data from the buffer.
    if (mBuffer.length() == mTransition.Size()) {
      return SetTransition(aFunc(nextState,
                                 mBuffer.begin(),
                                 mBuffer.length()));
    }

    // Anything else indicates a bug.
    MOZ_ASSERT_UNREACHABLE("Unexpected state encountered during yield");
    return SetTransition(Transition::TerminateFailure());
  }

  template <typename Func>
  Maybe<LexerResult> Truncated(SourceBufferIterator& aIterator,
                               Func aFunc)
  {
    // The data is truncated. Let the lexer clean up and decide which terminal
    // state we should end up in.
    LexerTransition<State> transition
      = mTruncatedTransition.NextStateIsTerminal()
      ? mTruncatedTransition
      : aFunc(mTruncatedTransition.NextState(), nullptr, 0);

    if (!transition.NextStateIsTerminal()) {
      MOZ_ASSERT_UNREACHABLE("Truncated state didn't lead to terminal state?");
      return SetTransition(Transition::TerminateFailure());
    }

    // If the SourceBuffer was completed with a failing state, we end in
    // TerminalState::FAILURE no matter what. This only happens in exceptional
    // situations like SourceBuffer itself encountering a failure due to OOM.
    if (NS_FAILED(aIterator.CompletionStatus())) {
      return SetTransition(Transition::TerminateFailure());
    }

    return SetTransition(transition);
  }

  Maybe<LexerResult> SetTransition(const LexerTransition<State>& aTransition)
  {
    // There should be no transitions while we're buffering for a buffered read
    // unless they're to terminal states. (The terminal state transitions would
    // generally be triggered by error handling code.)
    MOZ_ASSERT_IF(!mBuffer.empty(),
                  aTransition.NextStateIsTerminal() ||
                  mBuffer.length() == mTransition.Size());

    // Similarly, the only transitions allowed in the middle of an unbuffered
    // read are to a terminal state, or a yield to the same state. Otherwise, we
    // should remain in the same state until the unbuffered read completes.
    MOZ_ASSERT_IF(mUnbufferedState,
                  aTransition.NextStateIsTerminal() ||
                  (aTransition.ControlFlow() == ControlFlowStrategy::YIELD &&
                   aTransition.NextState() == mTransition.UnbufferedState()) ||
                  mUnbufferedState->mBytesRemaining == 0);

    // If this transition is a yield, save the next state and return. We'll
    // handle the rest when Lex() gets called again.
    if (!aTransition.NextStateIsTerminal() &&
        aTransition.ControlFlow() == ControlFlowStrategy::YIELD) {
      mYieldingToState = Some(aTransition.NextState());
      return Some(LexerResult(Yield::OUTPUT_AVAILABLE));
    }

    // Update our transition.
    mTransition = aTransition;

    // Get rid of anything left over from the previous state.
    mBuffer.clear();
    mYieldingToState = Nothing();
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
      , mBytesConsumedInCurrentChunk(0)
    { }

    size_t mBytesRemaining;
    size_t mBytesConsumedInCurrentChunk;
  };

  Vector<char, InlineBufferSize> mBuffer;
  LexerTransition<State> mTransition;
  const LexerTransition<State> mTruncatedTransition;
  Maybe<State> mYieldingToState;
  Maybe<UnbufferedState> mUnbufferedState;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_StreamingLexer_h
