var total="";
print("misc_bench");

var document_1 = { };
document_1.live = { };
document_1.live.age = { };
document_1.live.age.value = "june 18, 1961";
document_1.live.time1 = { };
document_1.live.time2 = { };
document_1.live.time3 = { };

var window = { };

function setTimeout()
{
}

function lifetimer(){
        var today = new Date()
        var BirthDay = new Date(document_1.live.age.value)
        var timeold = (today.getTime() - BirthDay.getTime());
        var sectimeold = timeold / 1000;
        var secondsold = Math.floor(sectimeold);
        var msPerDay = 24 * 60 * 60 * 1000 ;
        timeold = (today.getTime() - BirthDay.getTime());
        var e_daysold = timeold / msPerDay;
        var daysold = Math.floor(e_daysold);
        var e_hrsold = (e_daysold - daysold)*24;
        var hrsold = Math.floor(e_hrsold);
        var minsold = Math.floor((e_hrsold - hrsold)*60);
        document_1.live.time1.value = daysold
        document_1.live.time2.value = hrsold
        document_1.live.time3.value = minsold
        window.status = "Well at the moment you are " + secondsold + "............ seconds old.";
        var timerID = setTimeout("lifetimer()",1000)
}

function misc_1()
{
    var startTime = new Date();
    for (var i = 0; i < 5000; i++)
        lifetimer();
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("misc_1 : ", elapsedTime);
    total += elapsedTime;
}

//General Array Function
function MakeArray(n) {
   this.length = n;
   for (var i = 1; i <=n; i++) {
     this[i] = 0;
   }
}

//Initialize Days of Week Array
var days = new MakeArray(7);
days[0] = "Saturday"
days[1] = "Sunday"
days[2] = "Monday"
days[3] = "Tuesday"
days[4] = "Wednesday"
days[5] = "Thursday"
days[6] = "Friday"

//Initialize Months Array
var months = new MakeArray(12);
months[1] = "January"
months[2] = "February"
months[3] = "March"
months[4] = "April"
months[5] = "May"
months[6] = "June"
months[7] = "July"
months[8] = "August"
months[9] = "September"
months[10] = "October"
months[11] = "November"
months[12] = "December"

var form = { };
form.day = { };
form.day.value = "14";
form.month = { };
form.month.value = "2";
form.year = { };
form.year.value = "1961";
form.result1 = { };
form.result2 = { };

//Day of Week Function
function compute(form) {
   var val1 = parseInt(form.day.value, 10)
   if ((val1 < 0) || (val1 > 31)) {
      alert("Day is out of range")
   }
   var val2 = parseInt(form.month.value, 10)
   if ((val2 < 0) || (val2 > 12)) {
      alert("Month is out of range")
   }
   var val2x = parseInt(form.month.value, 10)
   var val3 = parseInt(form.year.value, 10)
   if (val3 < 1900) {
      alert("You're that old!")
   }
   if (val2 == 1) {
      val2x = 13;
      val3 = val3-1
   }
   if (val2 == 2) {
      val2x = 14;
      val3 = val3-1
   }
   var val4 = parseInt(((val2x+1)*3)/5, 10)
   var val5 = parseInt(val3/4, 10)
   var val6 = parseInt(val3/100, 10)
   var val7 = parseInt(val3/400, 10)
   var val8 = val1+(val2x*2)+val4+val3+val5-val6+val7+2
   var val9 = parseInt(val8/7, 10)
   var val0 = val8-(val9*7)
   form.result1.value = months[val2]+" "+form.day.value +", "+form.year.value
   form.result2.value = days[val0]
}

function misc_2()
{
    var startTime = new Date();
    for (var i = 0; i < 5000; i++) {
        compute(form); compute(form);
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("misc_2 : ", elapsedTime);
    total += elapsedTime;
}

// Chris Englmeier <machin@mindspring.com>
// feel free to use this script as long as this text remains intact
// http://www.geocities.com/SiliconValley/Heights/2052
// Copyright 1996 Chris Englmeier

var Input = { };
Input.value = 4096;

function Newton(Input){
   // f(x) = x*x - Input.value
   // f'(x) = 2*x
   var X = 1;
   var FX = X*X - Input.value;
   for (var i = 0; i < 10; i++) {
      X = X - FX/(2*X);
      FX = X*X - Input.value;
   }
   Input.value = X;
   return true;
}

function misc_3()
{
    var startTime = new Date();
    for (var i = 0; i < 5000; i++) {
        Newton(Input); Newton(Input); Newton(Input); Newton(Input); Newton(Input); Newton(Input);
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("misc_3 : ", elapsedTime);
    total += elapsedTime;
}

/*
     Copyright 1996, Infohiway, Inc.  Restricted use is hereby
     granted (commercial and personal OK) so long as this code
     is not *directly* sold and the copyright notice is buried
     somewhere deep in your HTML document_1.  A link to our site
     http://www.infohiway.com is always appreciated of course,
     but is absolutely and positively not necessary. ;-)   -->
*/
var l=0;
var r=0;
var tc="";
var al=" abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
+"123456789";
var ns="0123456789";

function iA(){
 // this.length=iA.arguments.length; // strict warning: deprecated arguments usage
 for (var i=0;i<arguments.length;i++){
  this[i]=iA.arguments[i];
 }
}
/* 5829 byte database of distances uses position in the
   string "al" to substitute for the numerics, saving a bit
   over 10k download to visitors. */
var a=new iA(61);
a[0]="Albany, New York*  tdjaddcdu jgybagc hhfdggqihbdhhbfdphrc"
+"khfevbniu aaqghamejgm zcmfBehflcn iclejcndafeao malcbfxidebdC"
+"bqiAfjdvesgBeChxid ni";
a[1]="Albuquerque, New Mexico*  n xfriiileidvbqgq p pcedmaoipbn"
+"ffdddigofbglidcu helgjepdgheihhhalijasgmhlblcl t riedhiqdsedf"
+"pcw mghbjbjdf gchakanereei";
a[2]="Atlanta, Georgia*  mcfer aevbkaiabie bdnhgagcbaeihbncifgc"
+"ndmipfifgiecd cahbsheeuidbchffh kbbedhheefhcjadcgercfhkgzfobx"
+"aegscj uexhzcfbie";
a[3]="Augusta, Maine*  fgxgoaCfbggclbjdkavblehglajatbvdogjfzcri"
+"yacgublbqengqbDcqgG lepcrcmbpdnbraeahiscqapbeiCbhfagGhueEdnfz"
+"dviFgGhFfgbrh";
a[4]="Baltimore, Maryland*  sbggxadccgegcfdbpggbcfebdcmfpdjdeas"
+"hmes cbn egjagejgx jdzefdiajigikag kdb bcmakehia wabeedB pczb"
+"hctgpczhBbAa dlh";
a[5]="Billings, Montana*  qhfav qbuhqbt dflcpatfpanbeeihoblffaf"
+"gu oinaqbv jdjenbldocnfybkfhcp rctdthkeiavcshl pfvgi cbifmafd"
+"obmakihcuaje";
a[6]="Birmingham, Alabama*  tglci ddefcinbffgccfegfdmchcgflgm p"
+"cjhg dhbedfgcrcdctecfbfgegdjgaiceihgagbhheehgphgimdyinavheaqi"
+"hfsiwgydgdhc";
a[7]="Boise, Idaho*  zivaxiudwegcqatcwisgoihdmfshl lafczarbretd"
+"xgnbfgqghescrbBgqdnhshuexiybndldzdxcihucAidcifdcpgcdpgihfeebw"
+"gmf";
a[8]="Boston, Massachusetts*  dgidgehesbj ffifh qet mdh whpevga"
+"arcicnfkfndAendDbifmdobjimijioabaehpingm cbzgegaaEdsaBglawhtb"
+"BhEcDbdepc";
a[9]="Buffalo, New York*  icdegao edaih ccmfoehecfsgkiqidbnieak"
+"bjijavhjfyheeibn fciegblfcgf lbjbl cfvbbbedzgncxegesdpcyczgyh"
+"dal ";
a[10]="Charleston, South Carolina*  dhbaqaiagcaafejiqbl hdq oas"
+"fhcjegbfgbdkavhhcxifag eijbmcedgcggddkdlgchffubfejeCeraAbhfui"
+"lhxbBbCcecld";
a[11]="Charleston, West Virginia*  bgnadgbecfagjemggid phkdpbfh"
+"khcbh ffgfucgcxabffcjaefhgdbiaeedajbidh dhshbchhyemgwheardmcw"
+"dz xgcdif";
a[12]="Charlotte, North Carolina*  pbgdeb iddjfohjffcpemhrbgdjc"
+"eefcciigv ghxadffcgdhckedbgbfbcak kdeceatce ifAhpezbgcthldxaA"
+"bAdchji";
a[13]="Cheyenne, Wyoming*  ihmcpfm hha fdldgighcerekajhlgqefhhe"
+"jgkbl kduej hhlfmhqer fie rgqfidnctfkhcbigidddjfkikilepdfb";
a[14]="Chicago, Illinois*  ceh cfibjbcdbhnffelgiajiaigejaedqhff"
+"tec edn  idadgibhahghcdhkegiqddhjiubiasebinblatiugtegagc";
a[15]="Cleveland, Ohio*  faadkimfffagqdj paefmacbidiahbtihgwhce"
+"gcleddgfecjfdgeajchcjedctcacgcxcldvfehqfnewixhwicfja";
a[16]="Columbia, South Carolina*  ecjcpbkagbpcncrdhajcfaf c jbu"
+"igfxbe fbfdhhladdfigbcijfkhddfatceijdBdpizaghublcwhAgAidhkg";
a[17]="Columbus, Ohio*  jeldffaip ieniffkgahh hcfhtbgcvdbaeikgd"
+"gghchibefefibgiifdhs aihgxdldvadbphmavfxcwedchg";
a[18]="Dallas/Ft Worth, Texas*  ghg kffbjiihpibehidajdealccbn h"
+"cdemdjbieffebofmebaffk ndj larhtdjipgfflfbgmeqeucmacg";
a[19]="Denver, Colorado*  fglhfihhbet jcjfl qdfagfidjckbjduajdi"
+"bkhlhqiqgfcedrhqdhanctglfdbjchfeciek lfmdpbeb";
a[20]="Des Moines, Iowa*  eikcdhibleiddhhcldb ncefqaeifboicgbef"
+"iihkbkeeeacmfjincghn rbfdp cgjgihqgrcr jfci";
a[21]="Detroit, Michigan*  pgibobgclhbhifjeggtbhhvichgbmicffied"
+"jgfegcjcgckgfatac i whkiv ecpgnewgx wcebif";
a[22]="El Paso, Texas*  nefgvigdnbjcohidgbidhbnejgs odnflhk ues"
+"efhlephtgddqhxfpcjdkgkhhgefgcl qbschd";
a[23]="Fargo, North Dakota*  kaofm hcmapifbodjdraidkct egbdkamh"
+"nenghgdcrancpikcqcnieaohhekemcrgrbnbmcgc";
a[24]="Grand Junction, Colorado*  uelbmcnasfhfeakeghmglgw likfn"
+"bngtdtbhbgitgsiehpiwdjeffgekabijiheihkcrhgc";
a[25]="Hartford, Connecticut*  qchdmejemdzfmdC hgl nbj mbj ncab"
+"dipdmgk baygdgb D rbB jhvgsaC DhCecfoc";
a[26]="Houston, Texas*  j dbhigdngdcodidegkikhkhghcepamedfhfiho"
+"akfmgsfvdmbrhghndb nisawgmgfb";
a[27]="Indianapolis, Indiana*  fghde rdfatfaadgkibgeibhh gcgagd"
+"faigffqccfjevdkatfbdogkithviveehfi";
a[28]="Jackson, Mississippi*  fafipcbfrbeibaiahcjfdabalbieefhgg"
+" kanfidoixemgthe pffgqfueydiggf";
a[29]="Jacksonville, Florida*  kdvdhbwegcficekanfefefiefbkembad"
+"het hcmaCfrczihhwajhwfAeCggblh";
a[30]="Kansas City, Kansas/Missouri*  mgdboheadhnhefddehhdlckfc"
+"eailekgldhgofrbfipdbfkaghoirfrfjdb ";
a[31]="Las Vegas, Nevada*  nfbgrgp ygr pfraqcygxhkam wexhbivaBf"
+"j kadepbdblicdegkhxbkh";
a[32]="Little Rock, Arkansas*  pieaadkfgghccedblejbcdehifkdmci "
+"uikdsad ndehpgsivejadd";
a[33]="Los Angeles, California*  uhraAbtgrftarfAizimdogxcA cixc"
+"Edifmfdgrdfimiaccikczemi";
a[34]="Louisville, Kentucky*  chjichgaagg ggfegeg hfgaqecik wal"
+"audbfpbk tiwewef g ";
a[35]="Memphis, Tennessee*  j fbiabadak hhdhfdghjanggfnfwaketcb"
+"hoegcraubwbheeh";
a[36]="Miami, Florida*  nfqgiahfmcigo pdbclcwekhpfFfuhD lcygmiz"
+"hDiG jfp ";
a[37]="Milwaukee, Wisconsin*  cdeejchiiehhe lbhgqgefkftbhdsfchn"
+"cliucugshhage";
a[38]="Minneapolis, Minnesota*  hcmelblcgichogl phhingqbehrcfck"
+"ilet shpejifc";
a[39]="Nashville, Tennessee*  eci fgfhgffigipgegl wglevbcbq ict"
+" wcxdffgd";
a[40]="New Orleans, Louisiana*  mdjdfijcfelco kdpdydo vcg qheer"
+"dvhyik he";
a[41]="New York City, New York*  cgnhlejiaaxechcbCaqbAaihuirbB "
+"CcBdbdnb";
a[42]="Norfolk, Virginia*  mgmbggbgwedbgcCgqfAii vboezhD Biaimg";
a[43]="Oklahoma City, Oklahoma*  dflcmiihkaqdrgheofe k dhmcpfsi"
+"mcaf";
a[44]="Omaha, Nebraska*  nal meicocpiecngddididpcpipikdc ";
a[45]="Orlando, Florida*  iithihn DgsgBdj wdkgxaBgDihemi";
a[46]="Philadelphia, Pennsylvania*  wgcadbBfpezei ueqdAgC Bbacmf";
a[47]="Phoenix, Arizona*  tiAelglcgcnhfej cegfngw jd";
a[48]="Pittsburgh, Pennsylvania*  fiyemfwgfasbnhxdzaybbeje";
a[49]="Portland, Maine*  FbshCglhyaucE FbDiefqa";
a[50]="Portland, Oregon*  lfehtfgfu jifdagAhqe";
a[51]="Rapid City, South Dakota*  lciegdleninekcoigc";
a[52]="Reno, Nevada*  riecqcf bcgcygnh";
a[53]="St Louis, Missouri*  mfiercubudhfdf";
a[54]="Salt Lake City, Utah*  mcgfgehetejb";
a[55]="San Antonio, Texas*  m qduhoifd";
a[56]="San Diego, California*  ealfz n ";
a[57]="San Francisco, California*  haBdq ";
a[58]="Seattle, Washington*  Abrf";
a[59]="Washington, DC*  lh";
a[60]="Wichita, Kansas*  ";
// For string to numerics on Win3.x
var b=new iA(4);
b[0]=1;
b[1]=10;
b[2]=100;
b[3]=1000;

var document_2 = { };
document_2.isn = { };
document_2.isn.isn1 = { };
document_2.isn.isn1.selectedIndex = 60;
document_2.isn.isn1.options = { };
document_2.isn.isn1.options[document_2.isn.isn1.selectedIndex] = { };
document_2.isn.isn1.options[document_2.isn.isn1.selectedIndex].value = "8";
document_2.isn.isn2 = { };
document_2.isn.isn2.selectedIndex = 0;
document_2.isn.isn2.options = { };
document_2.isn.isn2.options[document_2.isn.isn2.selectedIndex] = { };
document_2.isn.isn2.options[document_2.isn.isn2.selectedIndex].value = " ";
document_2.isn.rb = { };
document_2.isn.rb[1] = { };
document_2.isn.rb[1].checked = false;
document_2.isn.disp = { };

function getMiles(form){
 var lf=(document_2.isn.isn1.options[document_2.isn.isn1.selectedIndex].value);
 var rf=(document_2.isn.isn2.options[document_2.isn.isn2.selectedIndex].value);
 var l=al.indexOf(lf);
 var r=al.indexOf(rf);
 if (r<l){
  l=r;
  r=al.indexOf(lf);
  }
 var ls=a[l];
 var pos=ls.indexOf("*")
 var ls1="From "+ls.substring(0,pos)+" to "
 +a[r].substring(0,a[r].indexOf("*"))+" is about ";
 ls=ls.substring(pos+1,ls.length);
 tc=ls.substring(((r*2)-(l*2)),ls.length);
 if (document_2.isn.rb[1].checked){
  dispKm();
  }
 else{
  dispMi();
  }
}
function dispMi(){
 var ls1="";
 var ch=tc.charAt(0);
 ls1+=al.indexOf(ch);
 ch=tc.charAt(1);
 if (ch!=" "){
  ls1+=(al.indexOf(ch)*10)+" miles";
  } //1.613
  else ls1+="00"+" miles";
 document_2.isn.disp.value=ls1;
}
function dispKm(){
 ch=tc.charAt(0);
 km="";
 km+=al.indexOf(ch);
 ch=tc.charAt(1);
 if (ch!=" "){
  km+=(al.indexOf(ch)*10);
  } //1.613
  else{
   km+="00";
  }
  num=0;

  kml=km.length;
  for (var i=kml-1;i>-1;i--){
   cr=km.substring(i,i+1);
   pos=ns.indexOf(cr);
   num+=pos*b[kml-i-1];
  }
  km="";
  km+=num*1.613;
  pos=km.indexOf(".");
  if (pos>-1){
   km=km.substring(0,pos);
  }
  ls1+=km+" kilometers";
  document_2.isn.disp.value=ls1;
}

function misc_4()
{
    var startTime = new Date();
    for (var i = 0; i < 1000; i++) {
        getMiles();
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("misc_4 : ", elapsedTime);
    total += elapsedTime;
}

misc_1();
misc_2();
misc_3();
misc_4();
