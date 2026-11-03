const api=globalThis.browser??chrome;
let sidebar;
api.runtime.onMessage.addListener(message=>{
 if(message.resourceOracle!==true)return;
 return (async()=>{
  if(message.method==="ready")return {available:true};
  if(message.method==="create"){
   sidebar=await new Promise(resolve=>api.devtools.panels.elements.createSidebarPane("Owned Height Oracle",resolve));
   const available=typeof sidebar.setHeight==="function";
   return {available,returned:available?typeof sidebar.setHeight("75px"):null};
  }
  if(message.method==="height") {try{return {returned:typeof sidebar.setHeight(message.value)};}catch(error){return {errorName:error.name};}}
  if(message.method==="object"){await sidebar.setObject({owned:true},"Owned object");return {};}
  if(message.method==="expression"){await sidebar.setExpression("({owned:true})","Owned expression");return {};}
  if(message.method==="page"){await sidebar.setPage("sidebar.html");return {};}
  throw Error("Unknown authored method");
 })();
});
