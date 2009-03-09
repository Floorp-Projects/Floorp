var Fixtures = {
  js: {
    responseBody:   '$("content").update("<H2>Hello world!</H2>");', 
    'Content-Type': '           text/javascript     '
  },
  
  html: {
    responseBody: "Pack my box with <em>five dozen</em> liquor jugs! " +
      "Oh, how <strong>quickly</strong> daft jumping zebras vex..."
  },
  
  xml: {
    responseBody:   '<?xml version="1.0" encoding="UTF-8" ?><name attr="foo">bar</name>', 
    'Content-Type': 'application/xml'
  },
  
  json: {
    responseBody:   '{\n\r"test": 123}', 
    'Content-Type': 'application/json'
  },
  
  jsonWithoutContentType: {
    responseBody:   '{"test": 123}'
  },
  
  invalidJson: {
    responseBody:   '{});window.attacked = true;({}',
    'Content-Type': 'application/json'
  },
  
  headerJson: {
    'X-JSON': '{"test": "hello #éà"}'
  }
};

var responderCounter = 0;

// lowercase comparison because of MSIE which presents HTML tags in uppercase
var sentence = ("Pack my box with <em>five dozen</em> liquor jugs! " +
"Oh, how <strong>quickly</strong> daft jumping zebras vex...").toLowerCase();

var message = 'You must be running your tests from rake to test this feature.';