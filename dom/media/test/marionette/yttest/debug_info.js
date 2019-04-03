video.mozRequestDebugInfo().then(debugInfo => {
  try {
    debugInfo = debugInfo.replace(/\t/g, '').split(/\n/g);
    var JSONDebugInfo = "{";
      for(let g =0; g<debugInfo.length-1; g++){
          var pair = debugInfo[g].split(": ");
          JSONDebugInfo += '"' + pair[0] + '":"' + pair[1] + '",';
      }
      JSONDebugInfo = JSONDebugInfo.slice(0,JSONDebugInfo.length-1);
      JSONDebugInfo += "}";
      result["debugInfo"] = JSON.parse(JSONDebugInfo);
  } catch (err) {
    console.log(`Error '${err.toString()} in JSON.parse(${debugInfo})`);
    result["debugInfo"] = debugInfo;
  }
  result["debugInfo"] = debugInfo;
  resolve(result);
});
