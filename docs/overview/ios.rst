Firefox for iOS
===============

Firefox iOS is built natively for iOS, and doesn`t use Gecko.

Due to Apple`s restrictions for browsers on iOS, WKWebKit is how users will interact with the web,
and how developers interact with web elements. The chrome around WKWebKit, however, is built in
Swift. UI wise, it is a combination of UIKit and SwiftUI; due to supporting n-2 iOS versions, the
team is limited from moving over to SwiftUI fully.

Firefox uses several external Mozilla packages, namely:

* Mozilla Rust Components for various Rust based application components such as FXAClient, Nimbus, etc.
* Glean SDK for telemetry
* Telemetry (deprecated; not actively used, but not fully removed yet)

Documentation can be found in the project `wiki <https://github.com/mozilla-mobile/firefox-ios/wiki>`_.

WKWebView
---------

WKWebView is part of Apple`s WebKit framework. It supports a complete web browsing experience,
rendering HTML, CSS, and JavaScrip content alongside an app`s native views. It can also be thought
of as an API to help render web pages on Apple platforms. For more information on WKWebView itself,
please see `Apple's documentation <https://developer.apple.com/documentation/webkit/wkwebview>`_.
