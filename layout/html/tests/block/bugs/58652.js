
function openWindow() {
  popupWin = window.open('/help2/help_index.html', 'remote', 'width=600,height=400')
}

function goHtml(x){

parent.INFO.location="/out/"+x+".html";

}


function goAsp(x){
//alert(parent.NAV.document.acctForm.fsiid.value)
parent.INFO.location=x+"?UI="+parent.NAV.document.acctForm.fsiid.value;

}


function getHash(){
var hash=location.hash;
//alert(hash.substring(1,hash.length));
return hash.substring(1,hash.length);
}

function goHelp(){

if (document.topMenuForm.current.value.substring(1,6)=="level"){
parent.NAV.popWin("/tsnhelp/Q_Reilly.htm",640,480);
}
else popHelp();


}


function popHelp(ch){
if(window.helpWin && !window.helpWin.closed){
helpWin.focus();
//helpWin.replace(url);
}

else

{
helpWin = window.open('/help2/help_index.html','helpWin','toolbar=yes,location=no,directories=yes,menubar=yes,scrollbars=yes,resizable=yes,width=600,height=470')
}

}





function showMenu(x){
if(document.topMenuForm.current.value!=""){
hide(document.topMenuForm.current.value);
}

show(x);
document.topMenuForm.current.value=x;
}

function writeDate(){
var mA = new Array('January','February',
'March','April','May','June','July','August','September','October','November','December');

var mD = new Array('Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday');
var date= new Date();

document.write(mD[date.getDay()]+", "+mA[date.getMonth()]+" "+date.getDate());

}


var level=3;

function getCookieVal (offset){
var endstr = document.cookie.indexOf (";", offset);
if (endstr == -1)
endstr =document.cookie.length;
return decodeURIComponent(document.cookie.substring(offset, endstr));
}



function GetCookie (name) {
var arg = name + "=";
var alen = arg.length;
var clen = document.cookie.length;
var i = 0;
while (i < clen) {
var j = i + alen;
if (document.cookie.substring(i, j) == arg)
return getCookieVal (j);
i = document.cookie.indexOf(" ", i) + 1;
if (i == 0) break; 
}
return null;
}




function levelWrite (text, x) {
if(level>(x-1)){
document.write(text);
}
}


function show(id) {
ns4 = (document.layers)? true:false;
ie4 = (document.all)? true:false;
if (ns4) document.layers['menubox'].layers[id].visibility = "show"
else if (ie4) document.all[id].style.visibility = "visible"
}


function hide(id) {     
ns4 = (document.layers)? true:false;
ie4 = (document.all)? true:false;
if (ns4) document.layers['menubox'].layers[id].visibility = "hide"
else if (ie4) document.all[id].style.visibility = "hidden"
}



function ppii(x){
parent.INFO.location=x;
}






function showResLevel(){
showMenu(rlevel);
}




function ppiiEdu(x){
parent.INFO.location="/education/"+x;
}
