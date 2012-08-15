/*
 * Helper structures to track callbacks from image and channel loads.
 */

// One bit per callback that imageListener below implements. Stored in
// ImageListener.state.
// START_REQUEST and STOP_REQUEST are also reused by ChannelListener, and
// stored in ChannelListener.requestStatus.
const START_REQUEST = 0x01;
const START_DECODE = 0x02;
const START_CONTAINER = 0x04;
const START_FRAME = 0x08;
const STOP_FRAME = 0x10;
const STOP_CONTAINER = 0x20;
const STOP_DECODE = 0x40;
const STOP_REQUEST = 0x80;
const ALL_BITS = 0xFF;

// An implementation of imgIDecoderObserver with the ability to call specified
// functions on onStartRequest and onStopRequest.
function ImageListener(start_callback, stop_callback)
{
  this.onStartRequest = function onStartRequest(aRequest)
  {
    do_check_false(this.synchronous);

    this.state |= START_REQUEST;

    if (this.start_callback)
      this.start_callback(this, aRequest);
  }
  this.onStartDecode = function onStartDecode(aRequest)
  {
    do_check_false(this.synchronous);

    this.state |= START_DECODE;
  }
  this.onStartContainer = function onStartContainer(aRequest, aContainer)
  {
    do_check_false(this.synchronous);

    this.state |= START_CONTAINER;
  }
  this.onStartFrame = function onStartFrame(aRequest, aFrame)
  {
    do_check_false(this.synchronous);

    this.state |= START_FRAME;
  }
  this.onStopFrame = function onStopFrame(aRequest, aFrame)
  {
    do_check_false(this.synchronous);

    this.state |= STOP_FRAME;
  }
  this.onStopContainer = function onStopContainer(aRequest, aContainer)
  {
    do_check_false(this.synchronous);

    this.state |= STOP_CONTAINER;
  }
  this.onStopDecode = function onStopDecode(aRequest, status, statusArg)
  {
    do_check_false(this.synchronous);

    this.state |= STOP_DECODE;
  }
  this.onStopRequest = function onStopRequest(aRequest, aIsLastPart)
  {
    do_check_false(this.synchronous);

    // onStopDecode must always be called before, and with, onStopRequest. See
    // imgRequest::OnStopDecode for more information.
    do_check_true(!!(this.state & STOP_DECODE));

    // We have to cancel the request when we're done with it to break any
    // reference loops!
    aRequest.cancelAndForgetObserver(0);

    this.state |= STOP_REQUEST;

    if (this.stop_callback)
      this.stop_callback(this, aRequest);
  }

  // Initialize the synchronous flag to true to start. This must be set to
  // false before exiting to the event loop!
  this.synchronous = true;

  // A function to call when onStartRequest is called.
  this.start_callback = start_callback;

  // A function to call when onStopRequest is called.
  this.stop_callback = stop_callback;

  // The image load/decode state.
  // A bitfield that tracks which callbacks have been called. Takes the bits
  // defined above.
  this.state = 0;
}

function NS_FAILED(val)
{
  return !!(val & 0x80000000);
}

function ChannelListener()
{
  this.onStartRequest = function onStartRequest(aRequest, aContext)
  {
    if (this.outputListener)
      this.outputListener.onStartRequest(aRequest, aContext);

    this.requestStatus |= START_REQUEST;
  }

  this.onDataAvailable = function onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount)
  {
    if (this.outputListener)
      this.outputListener.onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
  }

  this.onStopRequest = function onStopRequest(aRequest, aContext, aStatusCode)
  {
    if (this.outputListener)
      this.outputListener.onStopRequest(aRequest, aContext, aStatusCode);

    // If we failed (or were canceled - failure is implied if canceled),
    // there's no use tracking our state, since it's meaningless.
    if (NS_FAILED(aStatusCode))
      this.requestStatus = 0;
    else
      this.requestStatus |= STOP_REQUEST;
  }

  // A listener to pass the notifications we get to.
  this.outputListener = null;

  // The request's status. A bitfield that holds one or both of START_REQUEST
  // and STOP_REQUEST, according to which callbacks have been called on the
  // associated request.
  this.requestStatus = 0;
}
