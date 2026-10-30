// This source is copied byte-for-byte into authored extension realms.
const api=globalThis.browser??chrome;
let targetId,targetURL;
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const snapshot=async()=>{
 const [value]=await api.scripting.executeScript({target:{tabId:targetId},func:()=>({registered:document.documentElement.getAttribute("data-registered-root"),direct:document.documentElement.getAttribute("data-direct-root"),outline:getComputedStyle(document.body).outlineWidth,border:getComputedStyle(document.body).borderTopWidth,href:location.href})});
 return value.result;
};
const ready=async()=>{const end=Date.now()+10000;while(Date.now()<end){try{const value=await snapshot();if(value.href===targetURL&&value.registered)return value;}catch{}await pause(25);}throw Error("Real target/registered script readiness");};
globalThis.runResourceOracle=async message=>{
 if(message.method==="register"){
  await api.scripting.registerContentScripts([{id:"owned-path",matches:["http://127.0.0.1/*"],js:["scripts/registered.js"],css:["styles/registered.css"],runAt:"document_start"}]);
  return {module:globalThis.resourceModule===true,worker:typeof ServiceWorkerGlobalScope!=="undefined"&&self instanceof ServiceWorkerGlobalScope,location:location.href,scripts:await api.scripting.getRegisteredContentScripts()};
 }
 if(message.method==="target") {targetURL=message.url;targetId=(await api.tabs.create({url:targetURL,active:false})).id;return {snapshot:await ready()};}
 if(message.method==="exercise"){
  const target={tabId:targetId},before=(await snapshot()).border;
  const result=await api.scripting.executeScript({target,files:["scripts/direct.js"]});
  await api.scripting.insertCSS({target,files:["styles/direct.css"]});
  const inserted=await api.scripting.executeScript({target,func:()=>({border:getComputedStyle(document.body).borderTopWidth})});
  await api.scripting.removeCSS({target,files:["styles/direct.css"]});
  const removed=await api.scripting.executeScript({target,func:()=>getComputedStyle(document.body).borderTopWidth});
  return {before,result,inserted,removed,snapshot:await snapshot()};
 }
 if(message.method==="update"){
  await api.scripting.updateContentScripts([{id:"owned-path",js:["scripts/updated.js"],css:["styles/updated.css"]}]);
  targetURL=new URL(targetURL);targetURL.searchParams.set("epoch","updated");targetURL=String(targetURL);
  await api.tabs.update(targetId,{url:targetURL});return {scripts:await api.scripting.getRegisteredContentScripts(),snapshot:await ready()};
 }
 if(message.method==="foreign"){
  try{await api.scripting.executeScript({target:{tabId:targetId},files:[message.file]});return {rejected:false};}catch{return {rejected:true};}
 }
 if(message.method==="bad-register"){
  try{await api.scripting.registerContentScripts([{id:"owned-batch-new",matches:["http://127.0.0.1/*"],js:["scripts/registered.js"]},{id:"owned-path",matches:["http://127.0.0.1/*"],js:["scripts/registered.js"]}]);return {rejected:false,scripts:await api.scripting.getRegisteredContentScripts()};}catch{return {rejected:true,scripts:await api.scripting.getRegisteredContentScripts()};}
 }
 if(message.method==="url-shaped-metadata"){
  let registrationResolved=false,error=null;
  try{await api.scripting.registerContentScripts([{id:"owned-url-shaped",matches:["http://127.0.0.1/*"],js:["https://example.invalid/never.js"]}]);registrationResolved=true;}catch(caught){error=String(caught);}
  const scripts=await api.scripting.getRegisteredContentScripts();
  if(scripts.some(script=>script.id==="owned-url-shaped"))await api.scripting.unregisterContentScripts({ids:["owned-url-shaped"]});
  return {registrationResolved,error,scripts,after:await api.scripting.getRegisteredContentScripts()};
 }
 if(message.method==="cleanup"){await api.scripting.unregisterContentScripts();if(targetId!==undefined){await api.tabs.remove(targetId);targetId=undefined;}return {scripts:await api.scripting.getRegisteredContentScripts()};}
 throw Error("Unknown authored method");
};
api.runtime.onMessage.addListener(message=>message.resourceOracle===true?globalThis.runResourceOracle(message):undefined);
