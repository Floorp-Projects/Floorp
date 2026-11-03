// Same authored public program in Chrome and native document/classic/module.
const api=globalThis.browser??chrome;
globalThis.runResourceOracle=async message=>{
 if(message.method==='cleanup'){
  await api.offscreen.closeDocument();
  const immediateRemaining=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT']});
  let remaining=immediateRemaining;
  const start=Date.now(),end=start+10000;
  while(remaining.length&&Date.now()<end){
   await new Promise(resolve=>setTimeout(resolve,25));
   remaining=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT']});
  }
  return {immediateRemaining,remaining,elapsed:Date.now()-start};
 }
 if(message.method!=='probe')throw Error('Unknown owned offscreen command');
 const requestedURL=api.runtime.getURL('offscreen.html');
 await api.offscreen.createDocument({url:requestedURL,reasons:['DOM_PARSER'],justification:'Parse an authored owned document for context identity regression'});
 let identity;
 const end=Date.now()+10000;
 while(Date.now()<end){
  try{identity=await api.runtime.sendMessage({ownedOffscreenIdentity:true,nonce:message.nonce});if(identity?.nonce===message.nonce)break;}catch{}
  await new Promise(resolve=>setTimeout(resolve,25));
 }
 if(identity?.nonce!==message.nonce)throw Error('Actual offscreen document identity deadline');
 const unfiltered=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT']});
 const filtered=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentUrls:[requestedURL]});
 const byId=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],contextIds:unfiltered.map(value=>value.contextId)});
 const byDocumentId=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentIds:unfiltered.map(value=>value.documentId)});
 const byReturnedOrigin=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentOrigins:unfiltered.map(value=>value.documentOrigin)});
 const foreign=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentUrls:['chrome-extension://'+'p'.repeat(32)+'/offscreen.html']});
 return {runtimeId:api.runtime.id,worker:typeof ServiceWorkerGlobalScope!=='undefined'&&self instanceof ServiceWorkerGlobalScope,module:globalThis.offscreenModule===true,requestedURL,urlOrigin:new URL(requestedURL).origin,identity,unfiltered,filtered,byId,byDocumentId,byReturnedOrigin,foreign};
};
api.runtime.onMessage.addListener(message=>message.resourceOracle===true?globalThis.runResourceOracle(message):undefined);
