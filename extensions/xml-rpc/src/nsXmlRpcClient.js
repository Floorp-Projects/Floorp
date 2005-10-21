/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla XML-RPC Client component.
 *
 * The Initial Developer of the Original Code is
 * Digital Creations 2, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Martijn Pieters <mj@digicool.com> (original author)
 *   Samuel Sieb <samuel@sieb.net> brought it up to date with
 *   current APIs and added authentication
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 *  nsXmlRpcClient XPCOM component
 *  Version: $Revision: 1.35 $
 *
 *  $Id: nsXmlRpcClient.js,v 1.35 2005/10/21 22:36:39 bzbarsky%mit.edu Exp $
 */

/*
 * Constants
 */
const XMLRPCCLIENT_CONTRACTID = '@mozilla.org/xml-rpc/client;1';
const XMLRPCCLIENT_CID =
    Components.ID('{37127241-1e6e-46aa-ba87-601d41bb47df}');
const XMLRPCCLIENT_IID = Components.interfaces.nsIXmlRpcClient;

const XMLRPCFAULT_CONTRACTID = '@mozilla.org/xml-rpc/fault;1';
const XMLRPCFAULT_CID =
    Components.ID('{691cb864-0a7e-448c-98ee-4a7f359cf145}');
const XMLRPCFAULT_IID = Components.interfaces.nsIXmlRpcFault;

const DEBUG = false;
const DEBUGPARSE = false;

/*
 * Class definitions
 */

/* The nsXmlRpcFault class constructor. */
function nsXmlRpcFault() {}

/* the nsXmlRpcFault class def */
nsXmlRpcFault.prototype = {
    faultCode: 0,
    faultString: '',

    init: function(faultCode, faultString) {
        this.faultCode = faultCode;
        this.faultString = faultString;
    },

    toString: function() {
        return '<XML-RPC Fault: (' + this.faultCode + ') ' +
            this.faultString + '>';
    },

    // nsISupports interface
    QueryInterface: function(iid) {
        if (!iid.equals(Components.interfaces.nsISupports) &&
            !iid.equals(XMLRPCFAULT_IID))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        return this;
    }
};

/* The nsXmlRpcClient class constructor. */
function nsXmlRpcClient() {}

/* the nsXmlRpcClient class def */
nsXmlRpcClient.prototype = {
    _serverUrl: null,
    _useAuth: false,
    _passwordTried: false,

    init: function(serverURL) {
        var ios = Components.classes["@mozilla.org/network/io-service;1"].
            getService(Components.interfaces.nsIIOService);
        var oURL = ios.newURI(serverURL, null, null);

        // Make sure it is a complete spec
        // Note that we don't care what the scheme is otherwise.
        // Should we care? POST works only on http and https..
        if (!oURL.scheme) oURL.scheme = 'http';
        if ((oURL.scheme != 'http') && (oURL.scheme != 'https'))
            throw Components.Exception('Only HTTP is supported');

        this._serverUrl = oURL;
    },

    setAuthentication: function(username, password){
        if ((typeof username == "string") &&
            (typeof password == "string")){
          this._useAuth = true;
          this._username = username;
          this._password = password;
          this._passwordTried = false;
        }
    },

    clearAuthentication: function(){
        this._useAuth = false;
    },

    get serverUrl() { return this._serverUrl; },

    // Internal copy of the status
    _status: null,
    _errorMsg: null,
    _listener: null,
    _seenStart: false,

    asyncCall: function(listener, context, methodName, methodArgs, count) {
        debug('asyncCall');
        // Check for call in progress.
        if (this._inProgress)
            throw Components.Exception('Call in progress!');

        // Check for the server URL;
        if (!this._serverUrl)
            throw Components.Exception('Not initilized');

        this._inProgress = true;

        // Clear state.
        this._status = null;
        this._errorMsg = null;
        this._listener = listener;
        this._seenStart = false;
        this._context = context;
        
        debug('Arguments: ' + methodArgs);

        // Generate request body
        var xmlWriter = new XMLWriter();
        this._generateRequestBody(xmlWriter, methodName, methodArgs);

        var requestBody = xmlWriter.data;

        debug('Request: ' + requestBody);

        var chann = this._getChannel(requestBody);

        // And...... call!
        chann.asyncOpen(this, context);
    },

    // Return a HTTP channel ready for POSTing.
    _getChannel: function(request) {
        // Set up channel.
        var ioService = getService('@mozilla.org/network/io-service;1',
            'nsIIOService');

        var chann = ioService.newChannelFromURI(this._serverUrl)
            .QueryInterface(Components.interfaces.nsIHttpChannel);

        // Create a stream out of the request and attach it to the channel
        var upload = chann.QueryInterface(Components.interfaces.nsIUploadChannel);
        var postStream = createInstance('@mozilla.org/io/string-input-stream;1',
            'nsIStringInputStream');
        postStream.setData(request, request.length);
        upload.setUploadStream(postStream, 'text/xml', -1);

        // Set the request method. setUploadStream guesses the method,
        // so we gotta do this afterwards.
        chann.requestMethod = 'POST';

        chann.notificationCallbacks = this;

        return chann;
    },

    // Flag indicating wether or not we are calling the server.
    _inProgress: false,
    get inProgress() { return this._inProgress; },

    // nsIStreamListener interface, so's we know about the pending request.
    onStartRequest: function(channel, ctxt) { 
        debug('Start Request') 
    }, // Do exactly nada.

    // End of the request
    onStopRequest: function(channel, ctxt, status) {
        debug('Stop Request');
        if (!this._inProgress) return; // No longer interested.

        this._inProgress = false;
        this._parser = null;
        
        if (status) {
            debug('Non-zero status: (' + status.toString(16) + ') ');
            this._status = status;
            this._errorMsg = errorMsg;
            try {
                this._listener.onError(this, ctxt, status,
                                       status.toString(16));
            } catch (ex) {
                debug('Exception in listener.onError: ' + ex);
            }
            return;
        }

        // All done.
        debug('Parse finished');
        if (this._foundFault) {
            try {
                this._fault = createInstance(XMLRPCFAULT_CONTRACTID,
                    'nsIXmlRpcFault');
                this._fault.init(this._result.getValue('faultCode').data,
                    this._result.getValue('faultString').data);
                this._result = null;
            } catch(e) {
                this._fault = null;
                this._result = null;
                throw Components.Exception('Could not parse response');
                try { 
                    this._listener.onError(this, ctxt, 
                        Components.results.NS_ERROR_FAIL, 
                        'Server returned invalid Fault');
                }
                catch(ex) {
                    debug('Exception in listener.onError: ' + ex);
                }
            }
            debug('Fault: ' + this._fault);
            try { this._listener.onFault(this, ctxt, this._fault); }
            catch(ex) {
                debug('Exception in listener.onFault: ' + ex);
            }
        } else if (this._result) { 
            debug('Result: ' + this._result);
            try { 
                this._listener.onResult(this, ctxt, this._result);
            } catch (ex) {
                debug('Exception in listener.onResult: ' + ex);
            }
        }
    },

    _parser: null,
    _foundFault: false,
    
    // Houston, we have data.
    onDataAvailable: function(channel, ctxt, inStr, sourceOffset, count) {
        debug('Data available (' + sourceOffset + ', ' + count + ')');
        if (!this._inProgress) return; // No longer interested.

        if (!this._seenStart) {
            // First time round
            this._seenStart = true;

            // Store request status and message.
            channel = channel
                .QueryInterface(Components.interfaces.nsIHttpChannel);
            this._responseStatus = channel.responseStatus;
            this._responseString = channel.responseString;

            // Check for a 200 response.
            if (channel.responseStatus != 200) {
                this._status = Components.results.NS_ERROR_FAILURE;
                this._errorMsg = 'Server returned unexpected status ' +
                    channel.responseStatus;
                this._inProgress = false;
                try {
                    this._listener.onError(this, ctxt,
                        Components.results.NS_ERROR_FAILURE,
                        'Server returned unexpected status ' +
                            channel.responseStatus);
                } catch (ex) {
                    debug('Exception in listener.onError: ' + ex);
                }
                return;
            }

            // check content type
            if (channel.contentType != 'text/xml') {
                this._status = Components.results.NS_ERROR_FAILURE;
                this._errorMsg = 'Server returned unexpected content-type ' +
                    channel.contentType;
                this._inProgress = false;
                try {
                    this._listener.onError(this, ctxt,
                        Components.results.NS_ERROR_FAILURE,
                        'Server returned unexpected content-type ' +
                            channel.contentType);
                } catch (ex) {
                    debug('Exception in listener.onError: ' + ex);
                }
                return;
            }

            debug('Viable response. Let\'s parse!');
            debug('Content length = ' + channel.contentLength);
            
            this._parser = new SimpleXMLParser(toScriptableStream(inStr),
                channel.contentLength);
            this._parser.setDocumentHandler(this);

            // Make sure state is clean
            this._valueStack = [];
            this._currValue = null;
            this._cdata = null;
            this._foundFault = false;
        }
        
        debug('Cranking up the parser, window = ' + count);
        try {
            this._parser.parse(count);
        } catch(ex) {
            debug('Parser exception: ' + ex);
            this._status = ex.result;
            this._errorMsg = ex.message;
            try {
                this._listener.onError(this, ctxt, ex.result, ex.message);
            } catch(ex) {
                debug('Exception in listener.onError: ' + ex);
            }
            this._inProgress = false;
            this._parser = null;
        }

    },

    _fault: null,
    _result: null,
    _responseStatus: null,
    _responseString: null,

    get fault() { return this._fault; },
    get result() { return this._result; },
    get responseStatus() { return this._responseStatus; },
    get responseString() { return this._responseString; },

    /* Convenience. Create an appropriate XPCOM object for a given type */
    INT:      1,
    BOOLEAN:  2,
    STRING:   3,
    DOUBLE:   4,
    DATETIME: 5,
    ARRAY:    6,
    STRUCT:   7,
    BASE64:   8, // Not part of nsIXmlRpcClient interface, internal use.
    createType: function(type, uuid) {
        const SUPPORTSID = '@mozilla.org/supports-';
        switch(type) {
            case this.INT:
                uuid.value = Components.interfaces.nsISupportsPRInt32
                return createInstance(SUPPORTSID + 'PRInt32;1',
                    'nsISupportsPRInt32');

            case this.BOOLEAN:
                uuid.value = Components.interfaces.nsISupportsPRBool
                return createInstance(SUPPORTSID + 'PRBool;1',
                    'nsISupportsPRBool');

            case this.STRING:
                uuid.value = Components.interfaces.nsISupportsCString
                return createInstance(SUPPORTSID + 'cstring;1',
                    'nsISupportsCString');

            case this.DOUBLE:
                uuid.value = Components.interfaces.nsISupportsDouble
                return createInstance(SUPPORTSID + 'double;1',
                    'nsISupportsDouble');

            case this.DATETIME:
                uuid.value = Components.interfaces.nsISupportsPRTime
                return createInstance(SUPPORTSID + 'PRTime;1',
                    'nsISupportsPRTime');

            case this.ARRAY:
                uuid.value = Components.interfaces.nsISupportsArray
                return createInstance(SUPPORTSID + 'array;1',
                    'nsISupportsArray');

            case this.STRUCT:
                uuid.value = Components.interfaces.nsIDictionary
                return createInstance('@mozilla.org/dictionary;1', 
                    'nsIDictionary');

            default: throw Components.Exception('Unsupported type');
        }
    },

    // nsISupports interface
    QueryInterface: function(iid) {
        if (!iid.equals(Components.interfaces.nsISupports) &&
            !iid.equals(XMLRPCCLIENT_IID) &&
            !iid.equals(Components.interfaces.nsIXmlRpcClientListener) &&
            !iid.equals(Components.interfaces.nsIRequestObserver) &&
            !iid.equals(Components.interfaces.nsIStreamListener) &&
            !iid.equals(Components.interfaces.nsIInterfaceRequestor))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        return this;
    },

    // nsIInterfaceRequester interface
    getInterface: function(iid, result){
        if (iid.equals(Components.interfaces.nsIAuthPrompt)){
            return this;
        }
        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    // nsIAuthPrompt interface
    _passwordTried: false,
    promptUsernameAndPassword: function(dialogTitle, text, passwordRealm,
                                        savePassword, user, pwd){

        if (this._useAuth){
            if (this._passwordTried){
                try { 
                    this._listener.onError(this, null, 
                        Components.results.NS_ERROR_FAIL, 
                        'Invalid credentials');
                }
                catch(ex) {
                    debug('Exception in listener.onError: ' + ex);
                }
                return false;
            }
            user.value = this._username;
            pwd.value = this._password;
            this._passwordTried = true;
            return true;
        }
        return false;
    },

    /* Generate the XML-RPC request body */
    _generateRequestBody: function(writer, methodName, methodArgs) {
        writer.startElement('methodCall');

        writer.startElement('methodName');
        writer.write(methodName);
        writer.endElement('methodName');

        writer.startElement('params');
        for (var i in methodArgs) {
            writer.startElement('param');
            this._generateArgumentBody(writer, methodArgs[i]);
            writer.endElement('param');
        }
        writer.endElement('params');

        writer.endElement('methodCall');
    },

    /* Write out a XML-RPC parameter value */
    _generateArgumentBody: function(writer, obj) {
        writer.startElement('value');
        var sType = this._typeOf(obj);
        switch (sType) {
            case 'PRUint8':
            case 'PRUint16':
            case 'PRInt16':
            case 'PRInt32':
                obj=obj.QueryInterface(Components.interfaces['nsISupports' +
                    sType]);
                writer.startElement('i4');
                writer.write(obj.toString());
                writer.endElement('i4');
                break;

            case 'PRBool':
                obj=obj.QueryInterface(Components.interfaces.nsISupportsPRBool);
                writer.startElement('boolean');
                writer.write(obj.data ? '1' : '0');
                writer.endElement('boolean');
                break;

            case 'Char':
            case 'CString':
                obj=obj.QueryInterface(Components.interfaces['nsISupports' +
                    sType]);
                writer.startElement('string');
                writer.write(obj.toString());
                writer.endElement('string');
                break;

            case 'Float':
            case 'Double':
                obj=obj.QueryInterface(Components.interfaces['nsISupports' +
                    sType]);
                writer.startElement('double');
                writer.write(obj.toString());
                writer.endElement('double');
                break;

            case 'PRTime':
                obj = obj.QueryInterface(
                    Components.interfaces.nsISupportsPRTime);
                var date = new Date(obj.data)
                writer.startElement('dateTime.iso8601');
                writer.write(iso8601Format(date));
                writer.endElement('dateTime.iso8601');
                break;
                
            case 'InputStream':
                obj = obj.QueryInterface(Components.interfaces.nsIInputStream);
                obj = toScriptableStream(obj);
                writer.startElement('base64');
                streamToBase64(obj, writer);
                writer.endElement('base64');
                break;
            
            case 'Array':
                obj = obj.QueryInterface(
                    Components.interfaces.nsISupportsArray);
                writer.startElement('array');
                writer.startElement('data');
                for (var i = 0; i < obj.Count(); i++)
                    this._generateArgumentBody(writer, obj.GetElementAt(i));
                writer.endElement('data');
                writer.endElement('array');
                break;

            case 'Dictionary':
                obj = obj.QueryInterface(Components.interfaces.nsIDictionary);
                writer.startElement('struct');
                var keys = obj.getKeys({});
                for (var k in keys) {
                    writer.startElement('member');
                    writer.startElement('name');
                    writer.write(keys[k]);
                    writer.endElement('name');
                    this._generateArgumentBody(writer, obj.getValue(keys[k]));
                    writer.endElement('member');
                }
                writer.endElement('struct');
                break;

            default:
                throw Components.Exception('Unsupported argument', null, null,
                    obj);
        }

        writer.endElement('value');
    },

    /* Determine type of a nsISupports primitive, array or dictionary. */
    _typeOf: function(obj) {
        // XPConnect alows JS to pass in anything, because we are a regular
        // JS object to it. So we have to test rigorously.
        if (typeof obj != 'object') return 'Unknown';

        // Anything else not nsISupports is not allowed.
        if (typeof obj.QueryInterface != 'function') return 'Unknown';

        // Now we will have to eliminate by trying all possebilities.
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsPRUint8);
            return 'PRUint8';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsPRUint16);
            return 'PRUint16';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsPRInt16);
            return 'PRInt16';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsPRInt32);
            return 'PRInt32';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsPRBool);
            return 'PRBool';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsChar);
            return 'Char';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsCString);
            return 'CString';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsFloat);
            return 'Float';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsDouble);
            return 'Double';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsPRTime);
            return 'PRTime';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsIInputStream);
            return 'InputStream';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsISupportsArray);
            return 'Array';
        } catch(e) {}
        
        try {
            obj.QueryInterface(Components.interfaces.nsIDictionary);
            return 'Dictionary';
        } catch(e) {}
        
        // Not a supported type
        return 'Unknown';
    },

    // Response parsing state
    _valueStack: [],
    _currValue: null,
    _cdata: null,

    /* SAX documentHandler interface (well, sorta) */
    characters: function(chars) {
        if (DEBUGPARSE) debug('character data: ' + chars);
        if (this._cdata == null) return;
        this._cdata += chars;
    },

    startElement: function(name) {
        if (DEBUGPARSE) debug('Start element ' + name);
        switch (name) {
            case 'fault':
                this._foundFault = true;
                break;

            case 'value':
                var val = new Value();
                this._valueStack.push(val);
                this._currValue = val;
                this._cdata = '';
                break;

            case 'name':
                this._cdata = '';
                break;

            case 'i4':
            case 'int':
                this._currValue.type = this.INT;
                break;

            case 'boolean':
                this._currValue.type = this.BOOLEAN;
                break;

            case 'double':
                this._currValue.type = this.DOUBLE;
                break;

            case 'dateTime.iso8601':
                this._currValue.type = this.DATETIME;
                break;

            case 'base64':
                this._currValue.type = this.BASE64;
                break;

            case 'struct':
                this._currValue.type = this.STRUCT;
                break;

            case 'array':
                this._currValue.type = this.ARRAY;
                break;
        }
    },

    endElement: function(name) {
        var val;
        if (DEBUGPARSE) debug('End element ' + name);
        switch (name) {
            case 'value':
                // take cdata and put it in this value;
                if (this._currValue.type != this.ARRAY &&
                    this._currValue.type != this.STRUCT) {
                    this._currValue.value = this._cdata;
                    this._cdata = null;
                }

                // Find out if this is the end value
                // Note that we treat a struct differently, see 'member'
                var depth = this._valueStack.length;
                if (depth < 2 || 
                    this._valueStack[depth - 2].type != this.STRUCT) {
                    val = this._currValue;
                    this._valueStack.pop();

                    if (depth < 2) {
                        if (DEBUG) debug('Found result');
                        // This is the top level object
                        this._result = val.value;
                        this._currValue = null;
                    } else {
                        // This is an array element. Add it.
                        this._currValue = 
                            this._valueStack[this._valueStack.length - 1];
                        this._currValue.appendValue(val.value);
                    }
                }
                break;
                
            case 'member':
                val = this._currValue;
                this._valueStack.pop();
                this._currValue = this._valueStack[this._valueStack.length - 1];
                this._currValue.appendValue(val.value);
                break;
            
            case 'name':
                this._currValue.name = this._cdata;
                this._cdata = null;
                break;
        }
    }
};

/* The XMLWriter class constructor */
function XMLWriter() {
    // We assume for now that all data is already in ISO-8859-1.
    this.data = '<?xml version="1.0" encoding="ISO-8859-1"?>';
}

/* The XMLWriter class def */
XMLWriter.prototype = {
    data: '',
    
    startElement: function(element) {
        this.data += '<' + element + '>';
    },

    endElement: function(element) {
        this.data += '</' + element + '>';
    },
    
    write: function(text) {
        for (var i = 0; i < text.length; i++) {
            var c = text[i];
            switch (c) {
                case '<':
                    this.data += '&lt;';
                    break;
                case '&':
                    this.data += '&amp;';
                    break;
                default:
                    this.data += c;
            }
        }
    },

    markup: function(text) { this.data += text }
};

/* The Value class contructor */
function Value() { this.type = this.STRING; };

/* The Value class def */
Value.prototype = {
    INT:      nsXmlRpcClient.prototype.INT,
    BOOLEAN:  nsXmlRpcClient.prototype.BOOLEAN,
    STRING:   nsXmlRpcClient.prototype.STRING,
    DOUBLE:   nsXmlRpcClient.prototype.DOUBLE,
    DATETIME: nsXmlRpcClient.prototype.DATETIME,
    ARRAY:    nsXmlRpcClient.prototype.ARRAY,
    STRUCT:   nsXmlRpcClient.prototype.STRUCT,
    BASE64:   nsXmlRpcClient.prototype.BASE64,
    
    _createType: nsXmlRpcClient.prototype.createType,

    name: null,
    
    _value: null,
    get value() { return this._value; },
    set value(val) {
        // accepts [0-9]+ or x[0-9a-fA-F]+ and returns the character.
        function entityTrans(substr, code) {
            return String.fromCharCode("0" + code);
        }
        
        switch (this.type) {
            case this.STRING:
                val = val.replace(/&#([0-9]+);/g, entityTrans);
                val = val.replace(/&#(x[0-9a-fA-F]+);/g, entityTrans);
                val = val.replace(/&lt;/g, '<');
                val = val.replace(/&gt;/g, '>');
                val = val.replace(/&amp;/g, '&');
                this._value.data = val;
                break;
        
            case this.BOOLEAN:
                this._value.data = (val == 1);
                break;

            case this.DATETIME:
                this._value.data = Date.UTC(val.slice(0, 4), 
                    val.slice(4, 6) - 1, val.slice(6, 8), val.slice(9, 11),
                    val.slice(12, 14), val.slice(15));
                break;

            case this.BASE64:
                this._value.data = base64ToString(val);
                break;

            default:
                this._value.data = val;
        }
    },

    _type: null,
    get type() { return this._type; },
    set type(type) { 
        this._type = type;
        if (type == this.BASE64) 
            this._value = this._createType(this.STRING, {});
        else this._value = this._createType(type, {});
    },

    appendValue: function(val) {
        switch (this.type) {
            case this.ARRAY:
                this.value.AppendElement(val);
                break;

            case this.STRUCT:
                this.value.setValue(this.name, val);
                break;
        }
    }
};

/* The SimpleXMLParser class constructor 
 * This parser is specific to the XML-RPC format!
 * It assumes tags without arguments, in lowercase.
 */
function SimpleXMLParser(instream, contentLength) {
    this._stream = new PushbackInputStream(instream);
    this._maxlength = contentLength;
}

/* The SimpleXMLParser class def */
SimpleXMLParser.prototype = {
    _stream: null,
    _docHandler: null,
    _bufferSize: 256,
    _parsed: 0,
    _maxlength: 0,
    _window: 0, // When async on big documents, release after windowsize.

    setDocumentHandler: function(handler) { this._docHandler = handler; },

    parse: function(windowsize) {
        this._window += windowsize;
        
        this._start();
    },

    // Guard maximum length
    _read: function(length) {
        length = Math.min(this._available(), length);
        if (!length) return '';
        var read = this._stream.read(length);
        this._parsed += read.length;
        return read;
    },
    _unread: function(data) {
        this._stream.unread(data);
        this._parsed -= data.length;
    },
    _available: function() {
        return Math.min(this._stream.available(), this._maxAvailable());
    },
    _maxAvailable: function() { return this._maxlength - this._parsed; },
    
    // read length characters from stream, block until we get them.
    _blockingRead: function(length) {
        length = Math.min(length, this._maxAvailable());
        if (!length) return '';
        var read = '';
        while (read.length < length) read += this._read(length - read.length);
        return read;
    },

    // read until the the 'findChar' character appears in the stream.
    // We read no more than _bufferSize characters, and return what we have
    // found so far, but no more than up to 'findChar' if found.
    _readUntil: function(findChar) {
        var read = this._blockingRead(this._bufferSize);
        var pos = read.indexOf(findChar.charAt(0));
        if (pos > -1) {
            this._unread(read.slice(pos + 1));
            return read.slice(0, pos + 1);
        }
        return read;
    },

    // Skip stream until string end is found.
    _skipUntil: function(end) {
        var read = '';
        while (this._maxAvailable()) {
            read += this._readUntil(end.charAt(0)) + 
                this._blockingRead(end.length - 1);
            var pos = read.indexOf(end);
            if (pos > -1) {
                this._unread(read.slice(pos + end.length));
                return;
            }
            read = read.slice(-(end.length)); // make sure don't miss our man.
        }
        return;
    },

    _buff: '',
    // keep track of whitespce, so's we can discard it.
    _killLeadingWS: false,
    _trailingWS: '',
    
    _start: function() {
        // parse until exhausted. Note that we only look at a window
        // of max. this._bufferSize. Also, parsing of comments, PI's and
        // CDATA isn't as solid as it could be. *shrug*, XML-RPC responses
        // are 99.99% of the time generated anyway.
        // We don't check well-formedness either. Errors in tags will
        // be caught at the doc handler.
        ParseLoop: while (this._maxAvailable() || this._buff) {
            // Check for window size. We stop parsing until more comes
            // available (only in async parsing).
            if (this._window < this._maxlength && 
                this._parsed >= this._window) 
                return;
        
            this._buff += this._read(this._bufferSize - this._buff.length);
            this._buff = this._buff.replace('\r\n', '\n');
            this._buff = this._buff.replace('\r', '\n');
            
            var startTag = this._buff.indexOf('<');
            var endTag;
            if (startTag > -1) {
                if (startTag > 0) { // We have character data.
                    var chars = this._buff.slice(0, startTag);
                    chars = chars.replace(/[ \t\n]*$/, '');
                    if (chars && this._killLeadingWS)
                        chars = chars.replace(/^[ \t\n]*/, '');
                    if (chars) {
                        // Any whitespace previously marked as trailing is in
                        // fact in the middle. Prepend.
                        chars = this._trailingWS + chars;
                        this._docHandler.characters(chars);
                    }
                    this._buff = this._buff.slice(startTag);
                    this._trailingWS = '';
                    this._killLeadingWS = false;
                }

                // Check for a PI
                if (this._buff.charAt(1) == '?') {
                    endTag = this._buff.indexOf('?>');
                    if (endTag > -1) this._buff = this._buff.slice(endTag + 2);
                    else {
                        // Make sure we don't miss '?' at the end of the buffer
                        this._unread(this._buff.slice(-1));
                        this._buff = '';
                        this._skipUntil('?>');
                    }
                    this._killLeadingWS = true;
                    continue;
                }

                // Check for a comment
                if (this._buff.slice(0, 4) == '<!--') {
                    endTag = this._buff.indexOf('-->');
                    if (endTag > -1) this._buff = this._buff.slice(endTag + 3);
                    else {
                        // Make sure we don't miss '--' at the end of the buffer
                        this._unread(this._buff.slice(-2));
                        this._buff = '';
                        this._skipUntil('-->');
                    }
                    this._killLeadingWS = true;
                    continue;
                }

                // Check for CDATA
                // We only check the first four characters. Anything longer and
                // we'd miss it and it would be recognized as a corrupt element
                // Anything shorter will be missed by the element scanner as
                // well. Next loop we'll have more characters to do a better
                // match.
                if (this._buff.slice(0, 4) == '<![C') {
                    // We need to be sure. If we have less than
                    // 9 characters in the buffer, we can't _be_ sure.
                    if (this._buff.length < 9 && this._maxAvailable()) continue;

                    if (this._buff.slice(0, 9) != '<![CDATA[')
                        throw Components.Exception('Error parsing response');
                    
                    endTag = this._buff.indexOf(']]>');
                    if (endTag > -1) {
                        this._buff = this._buff.slice(endTag + 3);
                        this._docHandler.characters(this._buff.slice(9, 
                            endTag));
                        this._killLeadingWS = true;
                        continue;
                    }  
                    
                    // end not in stream. Hrmph
                    this._docHandler.characters(this._buff.slice(9));
                    this._buff = '';
                    while(this._maxAvailable()) {
                        this._buff += this._readUntil(']') +
                            this._blockingRead(2);
                        // Find end.
                        var pos = this._buff.indexOf(']]>');
                        // Found.
                        if (pos > -1) {
                            this._docHandler.characters(this._buff.slice(0, 
                                pos));
                            this._buff = this._buff.slice(pos + 3);
                            this._killLeadingWS = true;
                            continue ParseLoop;
                        }
                        // Not yet found. Last 2 chars could be part of end.
                        this._docHandler.characters(this._buff.slice(0, -2));
                        this._buff = this._buff.slice(-2); 
                    }

                    if (this._buff) // Uhoh. No ]]> found before EOF.
                        throw Components.Exception('Error parsing response');

                    continue;
                }

                // Check for a DOCTYPE decl.
                if (this._buff.slice(0, 4) == '<!DO') {
                    if (this._buff.length < 9 && this.maxAvailable()) continue;

                    if (this._buff.slice(0, 9) != '<!DOCTYPE')
                        throw Components.Exception('Error parsing response');
                    
                    // Look for markup decl.
                    var startBrace = this._buff.indexOf('[');
                    if (startBrace > -1) {
                        this._unread(this._buff.slice(startBrace + 1));
                        this._buff = '';
                        this._skipUntil(']');
                        this._skipUntil('>');
                        this._killLeadingWS = true;
                        continue;
                    }
 
                    endTag = this._buff.indexOf('>');
                    if (endTag > -1) {
                        this._buff = this._buff.slice(endTag + 1);
                        this._killLeadingWS = true;
                        continue;
                    }

                    this._buff = '';
                    while(this._available()) {
                        this._buff = this._readUntil('>');

                        startBrace = this._buff.indexOf('[');
                        if (startBrace > -1) {
                            this._unread(this._buff.slice(startBrace + 1));
                            this._buff = '';
                            this._skipUntil(']');
                            this._skipUntil('>');
                            this._killLeadingWS = true;
                            continue ParseLoop;
                        }

                        endTag = this._buff.indexOf('>');
                        if (endTag > -1) {
                            this._buff = this._buff.slice(pos + 1);
                            this._killLeadingWS = true;
                            continue;
                        }
                    }

                    if (this._buff)
                        throw Components.Exception('Error parsing response');

                    continue;
                }
            
                endTag = this._buff.indexOf('>');
                if (endTag > -1) {
                    var tag = this._buff.slice(1, endTag);
                    this._buff = this._buff.slice(endTag + 1);
                    tag = tag.replace(/[ \t\n]+.*?(\/?)$/, '$1');

                    // XML-RPC tags are pretty simple.
                    if (/[^a-zA-Z0-9.\/]/.test(tag))
                        throw Components.Exception('Error parsing response');

                    // Determine start and/or end tag.
                    if (tag.charAt(tag.length - 1) == '/') {
                        this._docHandler.startElement(tag.slice(0, -1));
                        this._docHandler.endElement(tag.slice(0, -1));
                    } else if (tag.charAt(0) == '/') {
                        this._docHandler.endElement(tag.slice(1));
                    } else {
                        this._docHandler.startElement(tag);
                    }
                    this._killLeadingWS = true; 
                } else {
                    // No end tag. Check for window size to avoid an endless
                    // loop here.. hackish, I know, but if we get here this is
                    // not a XML-RPC request..
                    if (this._buff.length >= this._bufferSize)
                        throw Components.Exception('Error parsing response');
                    // If we get here and all what is to be read has
                    // been readinto the buffer, we have an incomplete stream.
                    if (!this._maxAvailable())
                        throw Components.Exception('Error parsing response');
                }
            } else {
                if (this._killLeadingWS) {
                    this._buff = this._buff.replace(/^[ \t\n]*/, '');
                    if (this._buff) this._killLeadingWS = false;
                } else {
                    // prepend supposed trailing whitespace to the front.
                    this._buff = this._trailingWS + this._buff;
                    this._trailingWS = '';
                }

                // store trailing whitespace, and only hand it over
                // the next time round. Unless we hit a tag, then we kill it
                if (this._buff) {
                    this._trailingWS = this._buff.match(/[ \t\n]*$/);
                    this._buff = this._buff.replace(/[ \t\n]*$/, '');
                }

                if (this._buff) this._docHandler.characters(this._buff);

                this._buff = '';
            }
        }
    }
};
                

/* The PushbackInputStream class constructor */
function PushbackInputStream(stream) {
    this._stream = stream;
}

/* The PushbackInputStream class def */
PushbackInputStream.prototype = {
    _stream: null,
    _read_characters: '',

    available: function() {
        return this._read_characters.length + this._stream.available();
    },

    read: function(length) {
        var read;
        if (this._read_characters.length >= length) {
            read = this._read_characters.slice(0, length);
            this._read_characters = this._read_characters.slice(length);
            return read;
        } else {
            read = this._read_characters;
            this._read_characters = '';
            return read + this._stream.read(length - read.length);
        }
    },

    unread: function(chars) { 
        this._read_characters = chars + this._read_characters;
    }
};
            
/*
 * Objects
 */

/* nsXmlRpcClient Module (for XPCOM registration) */
var nsXmlRpcClientModule = {
    registerSelf: function(compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

        compMgr.registerFactoryLocation(XMLRPCCLIENT_CID, 
                                        'XML-RPC Client JS component', 
                                        XMLRPCCLIENT_CONTRACTID, 
                                        fileSpec,
                                        location, 
                                        type);
        compMgr.registerFactoryLocation(XMLRPCFAULT_CID, 
                                        'XML-RPC Fault JS component', 
                                        XMLRPCFAULT_CONTRACTID, 
                                        fileSpec,
                                        location, 
                                        type);
    },

    getClassObject: function(compMgr, cid, iid) {
        if (!cid.equals(XMLRPCCLIENT_CID) && !cid.equals(XMLRPCFAULT_CID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        if (cid.equals(XMLRPCCLIENT_CID))
            return nsXmlRpcClientFactory
        else return nsXmlRpcFaultFactory;
    },

    canUnload: function(compMgr) { return true; }
};

/* nsXmlRpcClient Class Factory */
var nsXmlRpcClientFactory = {
    createInstance: function(outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
    
        if (!iid.equals(XMLRPCCLIENT_IID) &&
            !iid.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_INVALID_ARG;

        return new nsXmlRpcClient();
    }
}

/* nsXmlRpcFault Class Factory */
var nsXmlRpcFaultFactory = {
    createInstance: function(outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        if (!iid.equals(XMLRPCFAULT_IID) &&
            !iid.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_INVALID_ARG;

        return new nsXmlRpcFault();
    }
}

/*
 * Functions
 */

/* module initialisation */
function NSGetModule(comMgr, fileSpec) { return nsXmlRpcClientModule; }

/* Create an instance of the given ContractID, with given interface */
function createInstance(contractId, intf) {
    return Components.classes[contractId]
        .createInstance(Components.interfaces[intf]);
}

/* Get a pointer to a service indicated by the ContractID, with given interface */
function getService(contractId, intf) {
    return Components.classes[contractId].getService(Components.interfaces[intf]);
}

/* Convert an inputstream to a scriptable inputstream */
function toScriptableStream(input) {
    var SIStream = Components.Constructor(
        '@mozilla.org/scriptableinputstream;1',
        'nsIScriptableInputStream', 'init');
    return new SIStream(input);
}

/* format a Date object into a iso8601 datetime string, UTC time */
function iso8601Format(date) {
    var datetime = date.getUTCFullYear();
    var month = String(date.getUTCMonth() + 1);
    datetime += (month.length == 1 ?  '0' + month : month);
    var day = date.getUTCDate();
    datetime += (day < 10 ? '0' + day : day);

    datetime += 'T';

    var hour = date.getUTCHours();
    datetime += (hour < 10 ? '0' + hour : hour) + ':';
    var minutes = date.getUTCMinutes();
    datetime += (minutes < 10 ? '0' + minutes : minutes) + ':';
    var seconds = date.getUTCSeconds();
    datetime += (seconds < 10 ? '0' + seconds : seconds);

    return datetime;
}

/* Convert a stream to Base64, writing it away to a string writer */
const BASE64CHUNK = 255; // Has to be devidable by 3!!
function streamToBase64(stream, writer) {
    while (stream.available()) {
        var data = [];
        while (data.length < BASE64CHUNK && stream.available()) {
            var d = stream.read(1).charCodeAt(0);
            // reading a 0 results in NaN, compensate.
            data = data.concat(isNaN(d) ? 0 : d);
        }
        writer.write(toBase64(data));
    }
}

/* Convert data (an array of integers) to a Base64 string. */
const toBase64Table = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz' +
    '0123456789+/';
const base64Pad = '=';
function toBase64(data) {
    var result = '';
    var length = data.length;
    var i;
    // Convert every three bytes to 4 ascii characters.
    for (i = 0; i < (length - 2); i += 3) {
        result += toBase64Table[data[i] >> 2];
        result += toBase64Table[((data[i] & 0x03) << 4) + (data[i+1] >> 4)];
        result += toBase64Table[((data[i+1] & 0x0f) << 2) + (data[i+2] >> 6)];
        result += toBase64Table[data[i+2] & 0x3f];
    }

    // Convert the remaining 1 or 2 bytes, pad out to 4 characters.
    if (length%3) {
        i = length - (length%3);
        result += toBase64Table[data[i] >> 2];
        if ((length%3) == 2) {
            result += toBase64Table[((data[i] & 0x03) << 4) + (data[i+1] >> 4)];
            result += toBase64Table[(data[i+1] & 0x0f) << 2];
            result += base64Pad;
        } else {
            result += toBase64Table[(data[i] & 0x03) << 4];
            result += base64Pad + base64Pad;
        }
    }

    return result;
}

/* Convert Base64 data to a string */
const toBinaryTable = [
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
];
function base64ToString(data) {
    var result = '';
    var leftbits = 0; // number of bits decoded, but yet to be appended
    var leftdata = 0; // bits decoded, bt yet to be appended

    // Convert one by one.
    for (var i in data) {
        var c = toBinaryTable[data.charCodeAt(i) & 0x7f];
        var padding = (data[i] == base64Pad);
        // Skip illegal characters and whitespace
        if (c == -1) continue;
        
        // Collect data into leftdata, update bitcount
        leftdata = (leftdata << 6) | c;
        leftbits += 6;

        // If we have 8 or more bits, append 8 bits to the result
        if (leftbits >= 8) {
            leftbits -= 8;
            // Append if not padding.
            if (!padding)
                result += String.fromCharCode((leftdata >> leftbits) & 0xff);
            leftdata &= (1 << leftbits) - 1;
        }
    }

    // If there are any bits left, the base64 string was corrupted
    if (leftbits)
        throw Components.Exception('Corrupted base64 string');

    return result;
}

if (DEBUG) debug = function(msg) { 
    dump(' -- XML-RPC client -- : ' + msg + '\n'); 
};
else debug = function() {}

// vim:sw=4:sr:sta:et:sts:
