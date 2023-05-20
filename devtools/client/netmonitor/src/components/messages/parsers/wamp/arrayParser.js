/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// An implementation for WAMP messages parsing https://wamp-proto.org/
var WampMessageType;
(function (wampMessageTypeEnum) {
  wampMessageTypeEnum[(wampMessageTypeEnum.Hello = 1)] = "Hello";
  wampMessageTypeEnum[(wampMessageTypeEnum.Welcome = 2)] = "Welcome";
  wampMessageTypeEnum[(wampMessageTypeEnum.Abort = 3)] = "Abort";
  wampMessageTypeEnum[(wampMessageTypeEnum.Challenge = 4)] = "Challenge";
  wampMessageTypeEnum[(wampMessageTypeEnum.Authenticate = 5)] = "Authenticate";
  wampMessageTypeEnum[(wampMessageTypeEnum.Goodbye = 6)] = "Goodbye";
  wampMessageTypeEnum[(wampMessageTypeEnum.Error = 8)] = "Error";
  wampMessageTypeEnum[(wampMessageTypeEnum.Publish = 16)] = "Publish";
  wampMessageTypeEnum[(wampMessageTypeEnum.Published = 17)] = "Published";
  wampMessageTypeEnum[(wampMessageTypeEnum.Subscribe = 32)] = "Subscribe";
  wampMessageTypeEnum[(wampMessageTypeEnum.Subscribed = 33)] = "Subscribed";
  wampMessageTypeEnum[(wampMessageTypeEnum.Unsubscribe = 34)] = "Unsubscribe";
  wampMessageTypeEnum[(wampMessageTypeEnum.Unsubscribed = 35)] = "Unsubscribed";
  wampMessageTypeEnum[(wampMessageTypeEnum.Event = 36)] = "Event";
  wampMessageTypeEnum[(wampMessageTypeEnum.Call = 48)] = "Call";
  wampMessageTypeEnum[(wampMessageTypeEnum.Cancel = 49)] = "Cancel";
  wampMessageTypeEnum[(wampMessageTypeEnum.Result = 50)] = "Result";
  wampMessageTypeEnum[(wampMessageTypeEnum.Register = 64)] = "Register";
  wampMessageTypeEnum[(wampMessageTypeEnum.Registered = 65)] = "Registered";
  wampMessageTypeEnum[(wampMessageTypeEnum.Unregister = 66)] = "Unregister";
  wampMessageTypeEnum[(wampMessageTypeEnum.Unregistered = 67)] = "Unregistered";
  wampMessageTypeEnum[(wampMessageTypeEnum.Invocation = 68)] = "Invocation";
  wampMessageTypeEnum[(wampMessageTypeEnum.Interrupt = 69)] = "Interrupt";
  wampMessageTypeEnum[(wampMessageTypeEnum.Yield = 70)] = "Yield";
})(WampMessageType || (WampMessageType = {}));

// The WAMP protocol consists of many messages, disable complexity for one time
// eslint-disable-next-line complexity
function parseWampArray(messageArray) {
  const [messageType, ...args] = messageArray;
  switch (messageType) {
    case WampMessageType.Hello:
      return new HelloMessage(args);
    case WampMessageType.Welcome:
      return new WelcomeMessage(args);
    case WampMessageType.Abort:
      return new AbortMessage(args);
    case WampMessageType.Challenge:
      return new ChallengeMessage(args);
    case WampMessageType.Authenticate:
      return new AuthenticateMessage(args);
    case WampMessageType.Goodbye:
      return new GoodbyeMessage(args);
    case WampMessageType.Error:
      return new ErrorMessage(args);
    case WampMessageType.Publish:
      return new PublishMessage(args);
    case WampMessageType.Published:
      return new PublishedMessage(args);
    case WampMessageType.Subscribe:
      return new SubscribeMessage(args);
    case WampMessageType.Subscribed:
      return new SubscribedMessage(args);
    case WampMessageType.Unsubscribe:
      return new UnsubscribeMessage(args);
    case WampMessageType.Unsubscribed:
      return new UnsubscribedMessage(args);
    case WampMessageType.Event:
      return new EventMessage(args);
    case WampMessageType.Call:
      return new CallMessage(args);
    case WampMessageType.Cancel:
      return new CancelMessage(args);
    case WampMessageType.Result:
      return new ResultMessage(args);
    case WampMessageType.Register:
      return new RegisterMessage(args);
    case WampMessageType.Registered:
      return new RegisteredMessage(args);
    case WampMessageType.Unregister:
      return new UnregisterMessage(args);
    case WampMessageType.Unregistered:
      return new UnregisteredMessage(args);
    case WampMessageType.Invocation:
      return new InvocationMessage(args);
    case WampMessageType.Interrupt:
      return new InterruptMessage(args);
    case WampMessageType.Yield:
      return new YieldMessage(args);
    default:
      return null;
  }
}
class WampMessage {
  constructor(code) {
    this.messageCode = code;
    this.messageName = WampMessageType[code].toUpperCase();
  }
}
class HelloMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Hello);
    [this.realm, this.details] = messageArgs;
  }
}
class WelcomeMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Welcome);
    [this.session, this.details] = messageArgs;
  }
}
class AbortMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Abort);
    [this.details, this.reason] = messageArgs;
  }
}
class ChallengeMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Challenge);
    [this.authMethod, this.extra] = messageArgs;
  }
}
class AuthenticateMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Authenticate);
    [this.signature, this.extra] = messageArgs;
  }
}
class GoodbyeMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Goodbye);
    [this.details, this.reason] = messageArgs;
  }
}
class ErrorMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Error);
    [
      this.type,
      this.request,
      this.details,
      this.error,
      this.arguments,
      this.argumentsKw,
    ] = messageArgs;
  }
}
class PublishMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Publish);
    [this.request, this.options, this.topic, this.arguments, this.argumentsKw] =
      messageArgs;
  }
}
class PublishedMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Published);
    [this.request, this.publication] = messageArgs;
  }
}
class SubscribeMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Subscribe);
    [this.request, this.options, this.topic] = messageArgs;
  }
}
class SubscribedMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Subscribed);
    [this.request, this.subscription] = messageArgs;
  }
}
class UnsubscribeMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Unsubscribe);
    [this.request, this.subscription] = messageArgs;
  }
}
class UnsubscribedMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Unsubscribed);
    [this.request] = messageArgs;
  }
}
class EventMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Event);
    [
      this.subscription,
      this.publication,
      this.details,
      this.arguments,
      this.argumentsKw,
    ] = messageArgs;
  }
}
class CallMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Call);
    [
      this.request,
      this.options,
      this.procedure,
      this.arguments,
      this.argumentsKw,
    ] = messageArgs;
  }
}
class CancelMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Cancel);
    [this.request, this.options] = messageArgs;
  }
}
class ResultMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Result);
    [this.request, this.details, this.arguments, this.argumentsKw] =
      messageArgs;
  }
}
class RegisterMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Register);
    [this.request, this.options, this.procedure] = messageArgs;
  }
}
class RegisteredMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Registered);
    [this.request, this.registration] = messageArgs;
  }
}
class UnregisterMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Unregister);
    [this.request, this.registration] = messageArgs;
  }
}
class UnregisteredMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Unregistered);
    [this.request] = messageArgs;
  }
}
class InvocationMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Invocation);
    [
      this.request,
      this.registration,
      this.details,
      this.arguments,
      this.argumentsKw,
    ] = messageArgs;
  }
}
class InterruptMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Interrupt);
    [this.request, this.options] = messageArgs;
  }
}
class YieldMessage extends WampMessage {
  constructor(messageArgs) {
    super(WampMessageType.Yield);
    [this.request, this.options, this.arguments, this.argumentsKw] =
      messageArgs;
  }
}

module.exports = {
  parseWampArray,
};
