// Actual source APIs and Services preferences; explicit external writer models
// another window's persisted change. This is API-level, not UI interaction.
(async () => {
 const rows=[];
 const prefs=['floorp.design.configs','sidebar.verticalTabs','floorp.splitView.dragToSplitCreate.enabled'];
 function snapshot(){return prefs.map(name=>({name,user:Services.prefs.prefHasUserValue(name),type:Services.prefs.getPrefType(name),value:Services.prefs.getPrefType(name)===Services.prefs.PREF_BOOL?Services.prefs.getBoolPref(name):Services.prefs.getStringPref(name,'')}));}
 function restore(items){for(const x of items){if(!x.user)Services.prefs.clearUserPref(x.name);else if(x.type===Services.prefs.PREF_BOOL)Services.prefs.setBoolPref(x.name,x.value);else Services.prefs.setStringPref(x.name,x.value);}}
 const route={location:location.href,usesActor:qaKan24.usesSettingsActor(location.href,'5183')};
 if(route.usesActor)throw new Error('Expected actual chrome direct Services RPC route');
 for(const [native,stored,expected] of [[false,'vertical','horizontal'],[true,'horizontal','vertical']]){
  const saved=snapshot();let remove;let style=stored;const applied=[];
  try{Services.prefs.setBoolPref('sidebar.verticalTabs',native);remove=qaKan24.addVerticalTabsObserver(()=>style,next=>{style=next;applied.push(next);});rows.push({case:'initial-observer',native,stored,expected,actual:style,applied,passed:style===expected&&applied.length===1});}
  finally{remove?.();restore(saved);}
 }
 for(const [stale,latest] of [['horizontal','multirow'],['multirow','horizontal']]){
  const saved=snapshot();
  try{
   Services.prefs.setBoolPref('sidebar.verticalTabs',false);
   const initial=JSON.parse(Services.prefs.getStringPref('floorp.design.configs'));initial.tabbar.tabbarStyle=stale;
   Services.prefs.setStringPref('floorp.design.configs',JSON.stringify(initial));
   const form=await qaKan24.getDesignSettings();if(form.style!==stale)throw new Error('Stale form setup failed');
   const otherWindow=JSON.parse(Services.prefs.getStringPref('floorp.design.configs'));otherWindow.tabbar.tabbarStyle=latest;
   Services.prefs.setStringPref('floorp.design.configs',JSON.stringify(otherWindow));
   const beforeSave=JSON.parse(Services.prefs.getStringPref('floorp.design.configs')).tabbar.tabbarStyle;
   form.faviconColor=!form.faviconColor;
   await qaKan24.saveDesignSettings(form,{hasTabStyleChanged:false});
   const actual=JSON.parse(Services.prefs.getStringPref('floorp.design.configs'));
   rows.push({case:'stale-form-unrelated-save',stale,latest,beforeSave,actual:actual.tabbar.tabbarStyle,expected:latest,native:Services.prefs.getBoolPref('sidebar.verticalTabs'),unrelatedEditSaved:actual.globalConfigs.faviconColor===form.faviconColor,passed:actual.tabbar.tabbarStyle===latest&&actual.globalConfigs.faviconColor===form.faviconColor});
  }finally{restore(saved);}
 }
 return {route,rows,allPassed:rows.every(x=>x.passed)};
})()
