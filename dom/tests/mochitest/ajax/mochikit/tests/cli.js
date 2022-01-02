MochiKit = {__export__: true};
load('tests/FakeJSAN.js')
JSAN.use('MochiKit.MockDOM');
var window = this;
var document = MochiKit.MockDOM.document;
JSAN.use('MochiKit.MochiKit');
