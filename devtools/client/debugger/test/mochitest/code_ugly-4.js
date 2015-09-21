function a(){b()}function b(){debugger}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiYWJjLmpzIiwic291cmNlcyI6WyJkYXRhOnRleHQvamF2YXNjcmlwdCxmdW5jdGlvbiBhKCl7YigpfSIsImRhdGE6dGV4dC9qYXZhc2NyaXB0LGZ1bmN0aW9uIGIoKXtkZWJ1Z2dlcn0iXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsaUJDQUEsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMifQ==

// Generate this file by evaluating the following in a browser-environment
// scratchpad:
//
//    let { require } = Components.utils.import('resource://gre/modules/devtools/Loader.jsm', {});
//    let { SourceNode } = require("source-map");
//
//    let dataUrl = s => "data:text/javascript," + s;
//
//    let A = "function a(){b()}";
//    let A_URL = dataUrl(A);
//    let B = "function b(){debugger}";
//    let B_URL = dataUrl(B);
//
//    let result = (new SourceNode(null, null, null, [
//      new SourceNode(1, 0, A_URL, A),
//      B.split("").map((ch, i) => new SourceNode(1, i, B_URL, ch))
//    ])).toStringWithSourceMap({
//      file: "abc.js"
//    });
//
//    result.code + "\n//# " + "sourceMappingURL=data:application/json;base64," + btoa(JSON.stringify(result.map));

