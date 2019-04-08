video.mozRequestDebugInfo().then(debugInfo => {
  // The parsing won't be necessary once we have bug 1542674
  try {
    debugInfo = debugInfo.replace(/\t/g, '').split(/\n/g);
    var JSONDebugInfo = "{";
      for(let g =0; g<debugInfo.length-1; g++){
          var pair = debugInfo[g].split(": ");
          JSONDebugInfo += '"' + pair[0] + '":"' + pair[1] + '",';
      }
      JSONDebugInfo = JSONDebugInfo.slice(0,JSONDebugInfo.length-1);
      JSONDebugInfo += "}";
      result["mozRequestDebugInfo"] = JSON.parse(JSONDebugInfo);
  } catch (err) {
    console.log(`Error '${err.toString()} in JSON.parse(${debugInfo})`);
    result["mozRequestDebugInfo"] = debugInfo;
  }
  resolve(result);
});
