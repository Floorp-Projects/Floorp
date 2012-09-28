/*
 * This file is a modified version of netwerk/test/unit/head_channels.js
 * The changes consist of making it work in mochitest files and removing
 * unused code.
 */

/**
 * Read count bytes from stream and return as a String object
 */
function read_stream(stream, count) {
  /* assume stream has non-ASCII data */
  var wrapper =
      Components.classes["@mozilla.org/binaryinputstream;1"]
                .createInstance(Components.interfaces.nsIBinaryInputStream);
  wrapper.setInputStream(stream);
  /* JS methods can be called with a maximum of 65535 arguments, and input
     streams don't have to return all the data they make .available() when
     asked to .read() that number of bytes. */
  var data = [];
  while (count > 0) {
    var bytes = wrapper.readByteArray(Math.min(65535, count));
    data.push(String.fromCharCode.apply(null, bytes));
    count -= bytes.length;
    if (bytes.length == 0)
      throw("Nothing read from input stream!");
  }
  return data.join('');
}

const CL_EXPECT_FAILURE = 0x1;
const CL_EXPECT_GZIP = 0x2;
const CL_EXPECT_3S_DELAY = 0x4;
const CL_SUSPEND = 0x8;
const CL_ALLOW_UNKNOWN_CL = 0x10;
const CL_EXPECT_LATE_FAILURE = 0x20;

const SUSPEND_DELAY = 3000;

/**
 * A stream listener that calls a callback function with a specified
 * context and the received data when the channel is loaded.
 *
 * Signature of the closure:
 *   void closure(in nsIRequest request, in ACString data, in JSObject context);
 *
 * This listener makes sure that various parts of the channel API are
 * implemented correctly and that the channel's status is a success code
 * (you can pass CL_EXPECT_FAILURE or CL_EXPECT_LATE_FAILURE as flags
 * to allow a failure code)
 *
 * Note that it also requires a valid content length on the channel and
 * is thus not fully generic.
 */
function ChannelListener(closure, ctx, flags) {
  this._closure = closure;
  this._closurectx = ctx;
  this._flags = flags;
}
ChannelListener.prototype = {
  _closure: null,
  _closurectx: null,
  _buffer: "",
  _got_onstartrequest: false,
  _got_onstoprequest: false,
  _contentLen: -1,
  _lastEvent: 0,

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
    try {
      if (this._got_onstartrequest)
        throw("Got second onStartRequest event!");
      this._got_onstartrequest = true;
      this._lastEvent = Date.now();

      request.QueryInterface(Components.interfaces.nsIChannel);
      try {
        this._contentLen = request.contentLength;
      }
      catch (ex) {
        if (!(this._flags & (CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL)))
          throw("Could not get contentLength");
      }
      if (this._contentLen == -1 && !(this._flags & (CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL)))
        throw("Content length is unknown in onStartRequest!");

      if (this._flags & CL_SUSPEND) {
        request.suspend();
        do_timeout(SUSPEND_DELAY, function() { request.resume(); });
      }

    } catch (ex) {
      throw("Error in onStartRequest: " + ex);
    }
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    try {
      var current = Date.now();

      if (!this._got_onstartrequest)
        throw("onDataAvailable without onStartRequest event!");
      if (this._got_onstoprequest)
        throw("onDataAvailable after onStopRequest event!");
      if (!request.isPending())
        throw("request reports itself as not pending from onDataAvailable!");
      if (this._flags & CL_EXPECT_FAILURE)
        throw("Got data despite expecting a failure");

      if (current - this._lastEvent >= SUSPEND_DELAY &&
          !(this._flags & CL_EXPECT_3S_DELAY))
       throw("Data received after significant unexpected delay");
      else if (current - this._lastEvent < SUSPEND_DELAY &&
               this._flags & CL_EXPECT_3S_DELAY)
        throw("Data received sooner than expected");
      else if (current - this._lastEvent >= SUSPEND_DELAY &&
               this._flags & CL_EXPECT_3S_DELAY)
        this._flags &= ~CL_EXPECT_3S_DELAY; // No more delays expected

      this._buffer = this._buffer.concat(read_stream(stream, count));
      this._lastEvent = current;
    } catch (ex) {
      throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest: function(request, context, status) {
    try {
      var success = Components.isSuccessCode(status);
      if (!this._got_onstartrequest)
        throw("onStopRequest without onStartRequest event!");
      if (this._got_onstoprequest)
        throw("Got second onStopRequest event!");
      this._got_onstoprequest = true;
      if ((this._flags & (CL_EXPECT_FAILURE | CL_EXPECT_LATE_FAILURE)) && success)
        throw("Should have failed to load URL (status is " + status.toString(16) + ")");
      else if (!(this._flags & (CL_EXPECT_FAILURE | CL_EXPECT_LATE_FAILURE)) && !success)
        throw("Failed to load URL: " + status.toString(16));
      if (status != request.status)
        throw("request.status does not match status arg to onStopRequest!");
      if (request.isPending())
        throw("request reports itself as pending from onStopRequest!");
      if (!(this._flags & (CL_EXPECT_FAILURE | CL_EXPECT_LATE_FAILURE)) &&
          !(this._flags & CL_EXPECT_GZIP) &&
          this._contentLen != -1)
          is(this._buffer.length, this._contentLen);
    } catch (ex) {
      throw("Error in onStopRequest: " + ex);
    }
    try {
      this._closure(request, this._buffer, this._closurectx);
    } catch (ex) {
      throw("Error in closure function: " + ex);
    }
  }
};

/**
 * Class that implements nsILoadContext.  Use it as callbacks for channel when
 * test needs it.
 */
function LoadContextCallback(appId, inBrowserElement, isPrivate, isContent) {
  this.appId = appId;
  this.isInBrowserElement = inBrowserElement;
  this.usePrivateBrowsing = isPrivate;
  this.isContent = isContent;
}

LoadContextCallback.prototype = {
  associatedWindow: null,
  topWindow : null,
  isAppOfType: function(appType) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: function(iid) {
    if (iid == Ci.nsILoadContext ||
               Ci.nsIInterfaceRequestor ||
               Ci.nsISupports) {
        return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  getInterface: function(iid) {
    if (iid.equals(Ci.nsILoadContext))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
}
