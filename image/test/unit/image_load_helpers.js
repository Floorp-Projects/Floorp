/*
 * Helper structures to track callbacks from image and channel loads.
 */

// START_REQUEST and STOP_REQUEST are used by ChannelListener, and
// stored in ChannelListener.requestStatus.
const START_REQUEST = 0x01;
const STOP_REQUEST = 0x02;
const DATA_AVAILABLE = 0x04;

// One bit per callback that imageListener below implements. Stored in
// ImageListener.state.
const SIZE_AVAILABLE = 0x01;
const FRAME_UPDATE = 0x02;
const FRAME_COMPLETE = 0x04;
const LOAD_COMPLETE = 0x08;
const DECODE_COMPLETE = 0x10;

// Safebrowsing requires that the profile dir is set.
do_get_profile();

// An implementation of imgIScriptedNotificationObserver with the ability to
// call specified functions on onStartRequest and onStopRequest.
function ImageListener(start_callback, stop_callback)
{
  this.sizeAvailable = function onSizeAvailable(aRequest)
  {
    Assert.ok(!this.synchronous);

    this.state |= SIZE_AVAILABLE;

    if (this.start_callback)
      this.start_callback(this, aRequest);
  }
  this.frameComplete = function onFrameComplete(aRequest)
  {
    Assert.ok(!this.synchronous);

    this.state |= FRAME_COMPLETE;
  }
  this.decodeComplete = function onDecodeComplete(aRequest)
  {
    Assert.ok(!this.synchronous);

    this.state |= DECODE_COMPLETE;
  }
  this.loadComplete = function onLoadcomplete(aRequest)
  {
    Assert.ok(!this.synchronous);

    this.state |= LOAD_COMPLETE;

    if (this.stop_callback)
      this.stop_callback(this, aRequest);
  }
  this.frameUpdate = function onFrameUpdate(aRequest)
  {
  }
  this.isAnimated = function onIsAnimated()
  {
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

    this.requestStatus |= DATA_AVAILABLE;
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
