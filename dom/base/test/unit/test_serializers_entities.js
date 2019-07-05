const encoders = {
  xml: doc => {
    let enc = Cu.createDocumentEncoder("text/xml");
    enc.init(doc, "text/xml", Ci.nsIDocumentEncoder.OutputLFLineBreak);
    return enc;
  },
  html: doc => {
    let enc = Cu.createDocumentEncoder("text/html");
    enc.init(doc, "text/html", Ci.nsIDocumentEncoder.OutputLFLineBreak);
    return enc;
  },
  htmlBasic: doc => {
    let enc = Cu.createDocumentEncoder("text/html");
    enc.init(
      doc,
      "text/html",
      Ci.nsIDocumentEncoder.OutputEncodeBasicEntities |
        Ci.nsIDocumentEncoder.OutputLFLineBreak
    );
    return enc;
  },
  xhtml: doc => {
    let enc = Cu.createDocumentEncoder("application/xhtml+xml");
    enc.init(
      doc,
      "application/xhtml+xml",
      Ci.nsIDocumentEncoder.OutputLFLineBreak
    );
    return enc;
  },
};

// Which characters should we encode as entities?  It depends on the serializer.
const encodeAll = { html: true, htmlBasic: true, xhtml: true, xml: true };
const encodeHTMLBasic = {
  html: false,
  htmlBasic: true,
  xhtml: false,
  xml: false,
};
const encodeXML = { html: false, htmlBasic: false, xhtml: true, xml: true };
const encodeNone = { html: false, htmlBasic: false, xhtml: false, xml: false };
const encodingInfoMap = new Map([
  // Basic sanity chars '<', '>', '"', '&' get encoded in all cases.
  ["<", encodeAll],
  [">", encodeAll],
  ["&", encodeAll],
  // nbsp is only encoded with the HTML encoder when encoding basic entities.
  ["\xA0", encodeHTMLBasic],
]);

const encodingMap = new Map([
  ["<", "&lt;"],
  [">", "&gt;"],
  ["&", "&amp;"],
  // nbsp is only encoded with the HTML encoder when encoding basic entities.
  ["\xA0", "&nbsp;"],
]);

function encodingInfoForChar(c) {
  var info = encodingInfoMap.get(c);
  if (info) {
    return info;
  }
  return encodeNone;
}

function encodingForChar(c, type) {
  var info = encodingInfoForChar(c);
  if (!info[type]) {
    return c;
  }
  return encodingMap.get(c);
}

const doc = new DOMParser().parseFromString("<root></root>", "text/xml");
const root = doc.documentElement;
for (let i = 0; i < 255; ++i) {
  let el = doc.createElement("span");
  el.textContent = String.fromCharCode(i);
  root.appendChild(el);
}
for (let type of ["xml", "xhtml", "htmlBasic", "html"]) {
  let str = encoders[type](doc).encodeToString();
  const prefix = "<root><span>";
  const suffix = "</span></root>";
  Assert.ok(str.startsWith(prefix), `${type} serialization starts correctly`);
  Assert.ok(str.endsWith(suffix), `${type} serialization ends correctly`);
  str = str.substring(prefix.length, str.length - suffix.length);
  let encodings = str.split("</span><span>");
  for (let i = 0; i < 255; ++i) {
    let c = String.fromCharCode(i);
    Assert.equal(
      encodingForChar(c, type),
      encodings[i],
      `${type} encoding of char ${i} is correct`
    );
  }
}
