/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 459389;
var summary = 'Do not crash with JIT';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
print('mmmm, food!');


var SNI = {};
SNI.MetaData={};
SNI.MetaData.Parameter=function()
{
var parameters={}; 
this.addParameter=function(key,value)
{
parameters[key]=[];
parameters[key].push(value);
};
this.getParameter=function(key,separator){
if(!parameters[key])
{
return;
} 
return parameters[key].join(separator);
};
this.getKeys=function()
{
return parameters;
};
};
SNI.MetaData.Manager=function(){
var m=new SNI.MetaData.Parameter(); 
this.getParameter=m.getParameter; 
};
var MetaDataManager=SNI.MetaData.Manager;
SNI.Ads={ };
SNI.Ads.Url=function(){
var p=new SNI.MetaData.Parameter();
this.addParameter=p.addParameter;
this.getParameter=p.getParameter;
};
function Ad() {
var url=new SNI.Ads.Url();
this.addParameter=url.addParameter;
this.getParameter=url.getParameter;
}
function DartAd()
AdUrl.prototype=new Ad();
function AdUrl() { }
function AdRestriction() {
var p=new SNI.MetaData.Parameter();
this.addParameter=p.addParameter;
this.getParameter=p.getParameter;
this.getKeys=p.getKeys;
}
function AdRestrictionManager(){
this.restriction=[];
this.isActive=isActive;
this.isMatch=isMatch;
this.startMatch=startMatch;
function isActive(ad,mdm){
var value=false;
for(var i=0;i<this.restriction.length;i++)
{
adRestriction=this.restriction[i];
value=this.startMatch(ad,mdm,adRestriction);
} 
}
function startMatch(ad,mdm,adRestriction){
for(var key in adRestriction.getKeys()){
var restrictions=adRestriction.getParameter(key,',');
var value=mdm.getParameter(key,'');
match=this.isMatch(value,restrictions);
if(!match){
value=ad.getParameter(key,'')
match=this.isMatch(value,restrictions);
} 
} 
}
function isMatch(value,restrictions){
var match=false;
if(value)
{
splitValue=value.split('');
for(var x=0;x<splitValue.length;x++){
for(var a;a<restrictions.length;a++){ }
}
} 
return match;
}
}
var adRestrictionManager = new AdRestrictionManager();
var restrictionLeader6 = new AdRestriction();
restrictionLeader6.addParameter("", "");
adRestrictionManager.restriction.push(restrictionLeader6);
var restrictionLeader7 = new AdRestriction();
restrictionLeader7.addParameter("", "");
adRestrictionManager.restriction.push(restrictionLeader7);
function FoodAd(adtype)
{
  ad=new DartAd()
  ad.addParameter("",adtype)
  adRestrictionManager.isActive(ad,	mdManager)
}
var mdManager = new MetaDataManager() ;
  FoodAd('P')


reportCompare(expect, actual, summary);
