/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is JavaScript Engine testing utilities.
*
* The Initial Developer of the Original Code is Netscape Communications Corp.
* Portions created by the Initial Developer are Copyright (C) 2002
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com, Georgi Guninski
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK *****
*
*
* Date:    22 Jan 2002
* SUMMARY: Just seeing that we don't crash when compiling this script -
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=96526
*
*/
//-----------------------------------------------------------------------------
printBugNumber(96526);
printStatus("Just seeing that we don't crash when compiling this script -");


/*
 * Tail recursion test by Georgi Guninski
 */
a="[\"b\"]";
s="g";
for(i=0;i<20000;i++)
  s += a;
try {eval(s);}
catch (e) {};



/*
 * Function definition with lots of recursion, from http://www.newyankee.com
 */
function setaction(jumpto)
{
   if (jumpto == 0) window.location = "http://www.newyankee.com/GetYankees2.cgi?1.jpg";
   else if (jumpto == [0]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ImageName";
   else if (jumpto == [1]) window.location = "http://www.newyankee.com/GetYankees2.cgi?1.jpg";
   else if (jumpto == [2]) window.location = "http://www.newyankee.com/GetYankees2.cgi?arsrferguson.jpg";
   else if (jumpto == [3]) window.location = "http://www.newyankee.com/GetYankees2.cgi?akjamesmartin.jpg";
   else if (jumpto == [4]) window.location = "http://www.newyankee.com/GetYankees2.cgi?aldaverackett.jpg";
   else if (jumpto == [5]) window.location = "http://www.newyankee.com/GetYankees2.cgi?alericbrasher.jpg";
   else if (jumpto == [6]) window.location = "http://www.newyankee.com/GetYankees2.cgi?algeorgewatkins.jpg";
   else if (jumpto == [7]) window.location = "http://www.newyankee.com/GetYankees2.cgi?altoddcruise.jpg";
   else if (jumpto == [8]) window.location = "http://www.newyankee.com/GetYankees2.cgi?arkevinc.jpg";
   else if (jumpto == [9]) window.location = "http://www.newyankee.com/GetYankees2.cgi?arpaulmoore.jpg";
   else if (jumpto == [10]) window.location = "http://www.newyankee.com/GetYankees2.cgi?auphillaird.jpg";
   else if (jumpto == [11]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azbillhensley.jpg";
   else if (jumpto == [12]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azcharleshollandjr.jpg";
   else if (jumpto == [13]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azdaveholland.jpg";
   else if (jumpto == [14]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azdavidholland.jpg";
   else if (jumpto == [15]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azdonaldvogt.jpg";
   else if (jumpto == [16]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azernestortega.jpg";
   else if (jumpto == [17]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azjeromekeller.jpg";
   else if (jumpto == [18]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azjimpegfulton.jpg";
   else if (jumpto == [19]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azjohnbelcher.jpg";
   else if (jumpto == [20]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azmikejordan.jpg";
   else if (jumpto == [21]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azrickemry.jpg";
   else if (jumpto == [22]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azstephensavage.jpg";
   else if (jumpto == [23]) window.location = "http://www.newyankee.com/GetYankees2.cgi?azsteveferguson.jpg";
   else if (jumpto == [24]) window.location = "http://www.newyankee.com/GetYankees2.cgi?aztjhorrall.jpg";
   else if (jumpto == [25]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cabillmeiners.jpg";
   else if (jumpto == [26]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cabobhadley.jpg";
   else if (jumpto == [27]) window.location = "http://www.newyankee.com/GetYankees2.cgi?caboblennox.jpg";
   else if (jumpto == [28]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cabryanshurtz.jpg";
   else if (jumpto == [29]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cabyroncleveland.jpg";
   else if (jumpto == [30]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cacesarjimenez.jpg";
   else if (jumpto == [31]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cadalekirstine.jpg";
   else if (jumpto == [32]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cadavidlgoeffrion.jpg";
   else if (jumpto == [33]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cadennisnocerini.jpg";
   else if (jumpto == [34]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cadianemason.jpg";
   else if (jumpto == [35]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cadominicpieranunzio.jpg";
   else if (jumpto == [36]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cadonaldmotter.jpg";
   else if (jumpto == [37]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cadoncroner.jpg";
   else if (jumpto == [38]) window.location = "http://www.newyankee.com/GetYankees2.cgi?caelizabethwright.jpg";
   else if (jumpto == [39]) window.location = "http://www.newyankee.com/GetYankees2.cgi?caericlew.jpg";
   else if (jumpto == [40]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cafrancissmith.jpg";
   else if (jumpto == [41]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cafranklombano.jpg";
   else if (jumpto == [42]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cajaredweaver.jpg";
   else if (jumpto == [43]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cajerrythompson.jpg";
   else if (jumpto == [44]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cajimjanssen";
   else if (jumpto == [45]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cajohncopolillo.jpg";
   else if (jumpto == [46]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cajohnmessick.jpg";
   else if (jumpto == [47]) window.location = "http://www.newyankee.com/GetYankees2.cgi?calaynedicker.jpg";
   else if (jumpto == [48]) window.location = "http://www.newyankee.com/GetYankees2.cgi?caleeannrucker.jpg";
   else if (jumpto == [49]) window.location = "http://www.newyankee.com/GetYankees2.cgi?camathewsscharch.jpg";
   else if (jumpto == [50]) window.location = "http://www.newyankee.com/GetYankees2.cgi?camikedunn.jpg";
   else if (jumpto == [51]) window.location = "http://www.newyankee.com/GetYankees2.cgi?camikeshay.jpg";
   else if (jumpto == [52]) window.location = "http://www.newyankee.com/GetYankees2.cgi?camikeshepherd.jpg";
   else if (jumpto == [53]) window.location = "http://www.newyankee.com/GetYankees2.cgi?caphillipfreer.jpg";
   else if (jumpto == [54]) window.location = "http://www.newyankee.com/GetYankees2.cgi?carandy.jpg";
   else if (jumpto == [55]) window.location = "http://www.newyankee.com/GetYankees2.cgi?carichardwilliams.jpg";
   else if (jumpto == [56]) window.location = "http://www.newyankee.com/GetYankees2.cgi?carickgruen.jpg";
   else if (jumpto == [57]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cascottbartsch.jpg";
   else if (jumpto == [58]) window.location = "http://www.newyankee.com/GetYankees2.cgi?castevestrapac.jpg";
   else if (jumpto == [59]) window.location = "http://www.newyankee.com/GetYankees2.cgi?catimwest.jpg";
   else if (jumpto == [60]) window.location = "http://www.newyankee.com/GetYankees2.cgi?catomrietveld.jpg";
   else if (jumpto == [61]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnalainpaquette.jpg";
   else if (jumpto == [62]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnalanhill.jpg";
   else if (jumpto == [63]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnalguerette.jpg";
   else if (jumpto == [64]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnbrianhogg.jpg";
   else if (jumpto == [65]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnbrucebeard.jpg";
   else if (jumpto == [66]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cncraigdavey.jpg";
   else if (jumpto == [67]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cndanielpattison.jpg";
   else if (jumpto == [68]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cndenisstjean.jpg";
   else if (jumpto == [69]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnglenngray.jpg";
   else if (jumpto == [70]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnjeansebastienduguay.jpg";
   else if (jumpto == [71]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnjohnbritz.jpg";
   else if (jumpto == [72]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnkevinmclean.jpg";
   else if (jumpto == [73]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnmarcandrecartier.jpg";
   else if (jumpto == [74]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnmarcleblanc.jpg";
   else if (jumpto == [75]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnmatthewgiles.jpg";
   else if (jumpto == [76]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnmichelrauzon.jpg";
   else if (jumpto == [77]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnpierrelalonde.jpg";
   else if (jumpto == [78]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnraytyson.jpg";
   else if (jumpto == [79]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnrichardboucher.jpg";
   else if (jumpto == [80]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnrodbuike.jpg";
   else if (jumpto == [81]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnscottpitkeathly.jpg";
   else if (jumpto == [82]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnshawndavis.jpg";
   else if (jumpto == [83]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnstephanepelletier.jpg";
   else if (jumpto == [84]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cntodddesroches.jpg";
   else if (jumpto == [85]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cntonyharnum.jpg";
   else if (jumpto == [86]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cnwayneconabree.jpg";
   else if (jumpto == [87]) window.location = "http://www.newyankee.com/GetYankees2.cgi?codavidjbarber.jpg";
   else if (jumpto == [88]) window.location = "http://www.newyankee.com/GetYankees2.cgi?codonrandquist.jpg";
   else if (jumpto == [89]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cojeffpalese.jpg";
   else if (jumpto == [90]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cojohnlowell.jpg";
   else if (jumpto == [91]) window.location = "http://www.newyankee.com/GetYankees2.cgi?cotroytorgerson.jpg";
   else if (jumpto == [92]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ctgerrygranatowski.jpg";
   else if (jumpto == [93]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ctjasonklein.jpg";
   else if (jumpto == [94]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ctkevinkiss.jpg";
   else if (jumpto == [95]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ctmikekennedy.jpg";
   else if (jumpto == [96]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flalancanfield.jpg";
   else if (jumpto == [97]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flalbertgonzalez.jpg";
   else if (jumpto == [98]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flbruceloy.jpg";
   else if (jumpto == [99]) window.location = "http://www.newyankee.com/GetYankees2.cgi?fldandevault.jpg";
   else if (jumpto == [100]) window.location = "http://www.newyankee.com/GetYankees2.cgi?fldonstclair.jpg";
   else if (jumpto == [101]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flernestbonnell.jpg";
   else if (jumpto == [102]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flgeorgebarg.jpg";
   else if (jumpto == [103]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flgregslavinski.jpg";
   else if (jumpto == [104]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flgregwaters.jpg";
   else if (jumpto == [105]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flharoldmiller.jpg";
   else if (jumpto == [106]) window.location = "http://www.newyankee.com/GetYankees2.cgi?fljackwelch.jpg";
   else if (jumpto == [107]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flmichaelostrowski.jpg";
   else if (jumpto == [108]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flpauldoman.jpg";
   else if (jumpto == [109]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flpaulsessions.jpg";
   else if (jumpto == [110]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flrandymys.jpg";
   else if (jumpto == [111]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flraysarnowski.jpg";
   else if (jumpto == [112]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flrobertcahill.jpg";
   else if (jumpto == [113]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flstevemorrison.jpg";
   else if (jumpto == [114]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flstevezellner.jpg";
   else if (jumpto == [115]) window.location = "http://www.newyankee.com/GetYankees2.cgi?flterryjennings.jpg";
   else if (jumpto == [116]) window.location = "http://www.newyankee.com/GetYankees2.cgi?fltimmcwilliams.jpg";
   else if (jumpto == [117]) window.location = "http://www.newyankee.com/GetYankees2.cgi?fltomstellhorn.jpg";
   else if (jumpto == [118]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gabobkoch.jpg";
   else if (jumpto == [119]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gabrucekinney.jpg";
   else if (jumpto == [120]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gadickbesemer.jpg";
   else if (jumpto == [121]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gajackclunen.jpg";
   else if (jumpto == [122]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gajayhart.jpg";
   else if (jumpto == [123]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gajjgeller.jpg";
   else if (jumpto == [124]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gakeithlacey.jpg";
   else if (jumpto == [125]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gamargieminutello.jpg";
   else if (jumpto == [126]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gamarvinearnest.jpg";
   else if (jumpto == [127]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gamikeschwarz.jpg";
   else if (jumpto == [128]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gamikeyee.jpg";
   else if (jumpto == [129]) window.location = "http://www.newyankee.com/GetYankees2.cgi?garickdubree.jpg";
   else if (jumpto == [130]) window.location = "http://www.newyankee.com/GetYankees2.cgi?garobimartin.jpg";
   else if (jumpto == [131]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gastevewaddell.jpg";
   else if (jumpto == [132]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gathorwiggins.jpg";
   else if (jumpto == [133]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gawadewylie.jpg";
   else if (jumpto == [134]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gawaynerobinson.jpg";
   else if (jumpto == [135]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gepaulwestbury.jpg";
   else if (jumpto == [136]) window.location = "http://www.newyankee.com/GetYankees2.cgi?grstewartcwolfe.jpg";
   else if (jumpto == [137]) window.location = "http://www.newyankee.com/GetYankees2.cgi?gugregmesa.jpg";
   else if (jumpto == [138]) window.location = "http://www.newyankee.com/GetYankees2.cgi?hibriantokunaga.jpg";
   else if (jumpto == [139]) window.location = "http://www.newyankee.com/GetYankees2.cgi?himatthewgrady.jpg";
   else if (jumpto == [140]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iabobparnell.jpg";
   else if (jumpto == [141]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iadougleonard.jpg";
   else if (jumpto == [142]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iajayharmon.jpg";
   else if (jumpto == [143]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iajohnbevier.jpg";
   else if (jumpto == [144]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iamartywitt.jpg";
   else if (jumpto == [145]) window.location = "http://www.newyankee.com/GetYankees2.cgi?idjasonbartschi.jpg";
   else if (jumpto == [146]) window.location = "http://www.newyankee.com/GetYankees2.cgi?idkellyklaas.jpg";
   else if (jumpto == [147]) window.location = "http://www.newyankee.com/GetYankees2.cgi?idmikegagnon.jpg";
   else if (jumpto == [148]) window.location = "http://www.newyankee.com/GetYankees2.cgi?idrennieheuer.jpg";
   else if (jumpto == [149]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilbenshakman.jpg";
   else if (jumpto == [150]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilcraigstocks.jpg";
   else if (jumpto == [151]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ildaverubini.jpg";
   else if (jumpto == [152]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iledpepin.jpg";
   else if (jumpto == [153]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilfredkirpec.jpg";
   else if (jumpto == [154]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iljoecreed.jpg";
   else if (jumpto == [155]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iljohnknuth.jpg";
   else if (jumpto == [156]) window.location = "http://www.newyankee.com/GetYankees2.cgi?iljoshhill.jpg";
   else if (jumpto == [157]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilkeithrichard.jpg";
   else if (jumpto == [158]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilkrystleweber.jpg";
   else if (jumpto == [159]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilmattmusich.jpg";
   else if (jumpto == [160]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilmichaellane.jpg";
   else if (jumpto == [161]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilrodneyschwandt.jpg";
   else if (jumpto == [162]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilrogeraukerman.jpg";
   else if (jumpto == [163]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilscottbreeden.jpg";
   else if (jumpto == [164]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilscottgerami.jpg";
   else if (jumpto == [165]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilsteveritt.jpg";
   else if (jumpto == [166]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilthomasfollin.jpg";
   else if (jumpto == [167]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ilwaynesmith.jpg";
   else if (jumpto == [168]) window.location = "http://www.newyankee.com/GetYankees2.cgi?inallenwimberly.jpg";
   else if (jumpto == [169]) window.location = "http://www.newyankee.com/GetYankees2.cgi?inbutchmyers.jpg";
   else if (jumpto == [170]) window.location = "http://www.newyankee.com/GetYankees2.cgi?inderrickbentley.jpg";
   else if (jumpto == [171]) window.location = "http://www.newyankee.com/GetYankees2.cgi?inedmeissler.jpg";
   else if (jumpto == [172]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ingarymartin.jpg";
   else if (jumpto == [173]) window.location = "http://www.newyankee.com/GetYankees2.cgi?injasondavis.jpg";
   else if (jumpto == [174]) window.location = "http://www.newyankee.com/GetYankees2.cgi?injeffjones.jpg";
   else if (jumpto == [175]) window.location = "http://www.newyankee.com/GetYankees2.cgi?injeffwilliams.jpg";
   else if (jumpto == [176]) window.location = "http://www.newyankee.com/GetYankees2.cgi?injpreslyharrington.jpg";
   else if (jumpto == [177]) window.location = "http://www.newyankee.com/GetYankees2.cgi?inrichardlouden.jpg";
   else if (jumpto == [178]) window.location = "http://www.newyankee.com/GetYankees2.cgi?inronmorrell.jpg";
   else if (jumpto == [179]) window.location = "http://www.newyankee.com/GetYankees2.cgi?insearsweaver.jpg";
   else if (jumpto == [180]) window.location = "http://www.newyankee.com/GetYankees2.cgi?irpaullaverty.jpg";
   else if (jumpto == [181]) window.location = "http://www.newyankee.com/GetYankees2.cgi?irseamusmcbride.jpg";
   else if (jumpto == [182]) window.location = "http://www.newyankee.com/GetYankees2.cgi?isazrielmorag.jpg";
   else if (jumpto == [183]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ksalankreifels.jpg";
   else if (jumpto == [184]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ksbrianbudden.jpg";
   else if (jumpto == [185]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ksgarypahls.jpg";
   else if (jumpto == [186]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ksmikefarnet.jpg";
   else if (jumpto == [187]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ksmikethomas.jpg";
   else if (jumpto == [188]) window.location = "http://www.newyankee.com/GetYankees2.cgi?kstomzillig.jpg";
   else if (jumpto == [189]) window.location = "http://www.newyankee.com/GetYankees2.cgi?kybillyandrews.jpg";
   else if (jumpto == [190]) window.location = "http://www.newyankee.com/GetYankees2.cgi?kydaveryno.jpg";
   else if (jumpto == [191]) window.location = "http://www.newyankee.com/GetYankees2.cgi?kygreglaramore.jpg";
   else if (jumpto == [192]) window.location = "http://www.newyankee.com/GetYankees2.cgi?kywilliamanderson.jpg";
   else if (jumpto == [193]) window.location = "http://www.newyankee.com/GetYankees2.cgi?kyzachschuyler.jpg";
   else if (jumpto == [194]) window.location = "http://www.newyankee.com/GetYankees2.cgi?laadriankliebert.jpg";
   else if (jumpto == [195]) window.location = "http://www.newyankee.com/GetYankees2.cgi?labarryhumphus.jpg";
   else if (jumpto == [196]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ladennisanders.jpg";
   else if (jumpto == [197]) window.location = "http://www.newyankee.com/GetYankees2.cgi?larichardeckert.jpg";
   else if (jumpto == [198]) window.location = "http://www.newyankee.com/GetYankees2.cgi?laronjames.jpg";
   else if (jumpto == [199]) window.location = "http://www.newyankee.com/GetYankees2.cgi?lasheldonstutes.jpg";
   else if (jumpto == [200]) window.location = "http://www.newyankee.com/GetYankees2.cgi?lastephenstarbuck.jpg";
   else if (jumpto == [201]) window.location = "http://www.newyankee.com/GetYankees2.cgi?latroyestonich.jpg";
   else if (jumpto == [202]) window.location = "http://www.newyankee.com/GetYankees2.cgi?lavaughntrosclair.jpg";
   else if (jumpto == [203]) window.location = "http://www.newyankee.com/GetYankees2.cgi?maalexbrown.jpg";
   else if (jumpto == [204]) window.location = "http://www.newyankee.com/GetYankees2.cgi?maalwencl.jpg";
   else if (jumpto == [205]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mabrentmills.jpg";
   else if (jumpto == [206]) window.location = "http://www.newyankee.com/GetYankees2.cgi?madangodziff.jpg";
   else if (jumpto == [207]) window.location = "http://www.newyankee.com/GetYankees2.cgi?madanielwilusz.jpg";
   else if (jumpto == [208]) window.location = "http://www.newyankee.com/GetYankees2.cgi?madavidreis.jpg";
   else if (jumpto == [209]) window.location = "http://www.newyankee.com/GetYankees2.cgi?madougrecko.jpg";
   else if (jumpto == [210]) window.location = "http://www.newyankee.com/GetYankees2.cgi?majasonhaley.jpg";
   else if (jumpto == [211]) window.location = "http://www.newyankee.com/GetYankees2.cgi?maklausjensen.jpg";
   else if (jumpto == [212]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mamikemarland.jpg";
   else if (jumpto == [213]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mapetersilvestre.jpg";
   else if (jumpto == [214]) window.location = "http://www.newyankee.com/GetYankees2.cgi?maraysweeney.jpg";
   else if (jumpto == [215]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdallenbarnett.jpg";
   else if (jumpto == [216]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdcharleswasson.jpg";
   else if (jumpto == [217]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdedbaranowski.jpg";
   else if (jumpto == [218]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdfranktate.jpg";
   else if (jumpto == [219]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdfredschock.jpg";
   else if (jumpto == [220]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdianstjohn.jpg";
   else if (jumpto == [221]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdjordanevans.jpg";
   else if (jumpto == [222]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mdpaulwjones.jpg";
   else if (jumpto == [223]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mestevesandelier.jpg";
   else if (jumpto == [224]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mewilbertrbrown.jpg";
   else if (jumpto == [225]) window.location = "http://www.newyankee.com/GetYankees2.cgi?midavidkeller.jpg";
   else if (jumpto == [226]) window.location = "http://www.newyankee.com/GetYankees2.cgi?migaryvandenberg.jpg";
   else if (jumpto == [227]) window.location = "http://www.newyankee.com/GetYankees2.cgi?migeorgeberlinger.jpg";
   else if (jumpto == [228]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mijamesstapleton.jpg";
   else if (jumpto == [229]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mijerryhaney.jpg";
   else if (jumpto == [230]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mijohnrybarczyk.jpg";
   else if (jumpto == [231]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mikeithvalliere.jpg";
   else if (jumpto == [232]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mikevinpodsiadlik.jpg";
   else if (jumpto == [233]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mimarkandrews.jpg";
   else if (jumpto == [234]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mimikedecaussin.jpg";
   else if (jumpto == [235]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mimikesegorski.jpg";
   else if (jumpto == [236]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mirobertwolgast.jpg";
   else if (jumpto == [237]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mitimothybruner.jpg";
   else if (jumpto == [238]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mitomweaver.jpg";
   else if (jumpto == [239]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnbobgontarek.jpg";
   else if (jumpto == [240]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnbradbuffington.jpg";
   else if (jumpto == [241]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mndavewilson.jpg";
   else if (jumpto == [242]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mngenerajanen.jpg";
   else if (jumpto == [243]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnjohnkempkes.jpg";
   else if (jumpto == [244]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnkevinhurbanis.jpg";
   else if (jumpto == [245]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnmarklansink.jpg";
   else if (jumpto == [246]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnpaulmayer.jpg";
   else if (jumpto == [247]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnpauloman.jpg";
   else if (jumpto == [248]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mnwoodylobnitz.jpg";
   else if (jumpto == [249]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mocurtkempf.jpg";
   else if (jumpto == [250]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mojerryhenry.jpg";
   else if (jumpto == [251]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mojimfinney.jpg";
   else if (jumpto == [252]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mojimrecamper.jpg";
   else if (jumpto == [253]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mojohntimmons.jpg";
   else if (jumpto == [254]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mojohnvaughan.jpg";
   else if (jumpto == [255]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mokenroberts.jpg";
   else if (jumpto == [256]) window.location = "http://www.newyankee.com/GetYankees2.cgi?momacvoss.jpg";
   else if (jumpto == [257]) window.location = "http://www.newyankee.com/GetYankees2.cgi?momarktemmer.jpg";
   else if (jumpto == [258]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mopaulzerjav.jpg";
   else if (jumpto == [259]) window.location = "http://www.newyankee.com/GetYankees2.cgi?morobtigner.jpg";
   else if (jumpto == [260]) window.location = "http://www.newyankee.com/GetYankees2.cgi?motomantrim.jpg";
   else if (jumpto == [261]) window.location = "http://www.newyankee.com/GetYankees2.cgi?mscharleshahn.jpg";
   else if (jumpto == [262]) window.location = "http://www.newyankee.com/GetYankees2.cgi?msjohnjohnson.jpg";
   else if (jumpto == [263]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ncandrelopez.jpg";
   else if (jumpto == [264]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ncedorisak.jpg";
   else if (jumpto == [265]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ncjimisbell.jpg";
   else if (jumpto == [266]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ncjohnnydark.jpg";
   else if (jumpto == [267]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nckevinebert.jpg";
   else if (jumpto == [268]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nckevinulmer.jpg";
   else if (jumpto == [269]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ncpeteparis.jpg";
   else if (jumpto == [270]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ncstevelindsley.jpg";
   else if (jumpto == [271]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nctimsmith.jpg";
   else if (jumpto == [272]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nctonylawrence.jpg";
   else if (jumpto == [273]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ncwyneaston.jpg";
   else if (jumpto == [274]) window.location = "http://www.newyankee.com/GetYankees2.cgi?neberniedevlin.jpg";
   else if (jumpto == [275]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nebrentesmoil.jpg";
   else if (jumpto == [276]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nescottmccullough.jpg";
   else if (jumpto == [277]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhalantarring.jpg";
   else if (jumpto == [278]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhbjmolinari.jpg";
   else if (jumpto == [279]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhbrianmolinari.jpg";
   else if (jumpto == [280]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhdanhorning.jpg";
   else if (jumpto == [281]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhdonblackden.jpg";
   else if (jumpto == [282]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhjimcalandriello.jpg";
   else if (jumpto == [283]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhjohngunterman.jpg";
   else if (jumpto == [284]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nhjohnmagyar.jpg";
   else if (jumpto == [285]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njbudclarity.jpg";
   else if (jumpto == [286]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njcraigjones.jpg";
   else if (jumpto == [287]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njericrowland.jpg";
   else if (jumpto == [288]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njjimsnyder.jpg";
   else if (jumpto == [289]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njlarrylevinson.jpg";
   else if (jumpto == [290]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njlouisdispensiere.jpg";
   else if (jumpto == [291]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njmarksoloff.jpg";
   else if (jumpto == [292]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njmichaelhalko.jpg";
   else if (jumpto == [293]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njmichaelmalkasian.jpg";
   else if (jumpto == [294]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njnigelmartin.jpg";
   else if (jumpto == [295]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njrjmolinari.jpg";
   else if (jumpto == [296]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njtommurasky.jpg";
   else if (jumpto == [297]) window.location = "http://www.newyankee.com/GetYankees2.cgi?njtomputnam.jpg";
   else if (jumpto == [298]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nmdalepage.jpg";
   else if (jumpto == [299]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nmmikethompson.jpg";
   else if (jumpto == [300]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nvclydekemp.jpg";
   else if (jumpto == [301]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nvharveyklene.jpg";
   else if (jumpto == [302]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nvlonsimons.jpg";
   else if (jumpto == [303]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyabeweisfelner.jpg";
   else if (jumpto == [304]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyanthonygiudice.jpg";
   else if (jumpto == [305]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyaustinpierce.jpg";
   else if (jumpto == [306]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nybrianmonks.jpg";
   else if (jumpto == [307]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nycharlieporter.jpg";
   else if (jumpto == [308]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nycorneliuswoglum.jpg";
   else if (jumpto == [309]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nydennishartwell.jpg";
   else if (jumpto == [310]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nydennissgheerdt.jpg";
   else if (jumpto == [311]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nygeorgepettitt.jpg";
   else if (jumpto == [312]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyjohndrewes.jpg";
   else if (jumpto == [313]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyjohnminichiello.jpg";
   else if (jumpto == [314]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nykevinwoolever.jpg";
   else if (jumpto == [315]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nymartyrubinstein.jpg";
   else if (jumpto == [316]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyraysicina.jpg";
   else if (jumpto == [317]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyrobbartley.jpg";
   else if (jumpto == [318]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nyrobertkosty.jpg";
   else if (jumpto == [319]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nystephenbagnato.jpg";
   else if (jumpto == [320]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nystevegiamundo.jpg";
   else if (jumpto == [321]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nystevekelly.jpg";
   else if (jumpto == [322]) window.location = "http://www.newyankee.com/GetYankees2.cgi?nywayneadelkoph.jpg";
   else if (jumpto == [323]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohbriannimmo.jpg";
   else if (jumpto == [324]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohdavehyman.jpg";
   else if (jumpto == [325]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohdavidconant.jpg";
   else if (jumpto == [326]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohdennismantovani.jpg";
   else if (jumpto == [327]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohgrahambennett.jpg";
   else if (jumpto == [328]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohgregbrunk.jpg";
   else if (jumpto == [329]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohgregfilbrun.jpg";
   else if (jumpto == [330]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohjimreutener.jpg";
   else if (jumpto == [331]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohjimrike.jpg";
   else if (jumpto == [332]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohkeithsparks.jpg";
   else if (jumpto == [333]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohkevindrinan.jpg";
   else if (jumpto == [334]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohmichaelhaines.jpg";
   else if (jumpto == [335]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohmichaelsteele.jpg";
   else if (jumpto == [336]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohpatrickguanciale.jpg";
   else if (jumpto == [337]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohscottkelly.jpg";
   else if (jumpto == [338]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohscottthomas.jpg";
   else if (jumpto == [339]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohstevetuckerman.jpg";
   else if (jumpto == [340]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohtedfigurski.jpg";
   else if (jumpto == [341]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohterrydonald.jpg";
   else if (jumpto == [342]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohtimokeefe.jpg";
   else if (jumpto == [343]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ohtomhaydock.jpg";
   else if (jumpto == [344]) window.location = "http://www.newyankee.com/GetYankees2.cgi?okbillsneller.jpg";
   else if (jumpto == [345]) window.location = "http://www.newyankee.com/GetYankees2.cgi?okbobbulick.jpg";
   else if (jumpto == [346]) window.location = "http://www.newyankee.com/GetYankees2.cgi?okdaryljones.jpg";
   else if (jumpto == [347]) window.location = "http://www.newyankee.com/GetYankees2.cgi?okstevetarchek.jpg";
   else if (jumpto == [348]) window.location = "http://www.newyankee.com/GetYankees2.cgi?okwoodymcelroy.jpg";
   else if (jumpto == [349]) window.location = "http://www.newyankee.com/GetYankees2.cgi?orcoryeells.jpg";
   else if (jumpto == [350]) window.location = "http://www.newyankee.com/GetYankees2.cgi?oredcavasso.jpg";
   else if (jumpto == [351]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ormarkmcculley.jpg";
   else if (jumpto == [352]) window.location = "http://www.newyankee.com/GetYankees2.cgi?orstevekarthauser.jpg";
   else if (jumpto == [353]) window.location = "http://www.newyankee.com/GetYankees2.cgi?paalanpalmieri.jpg";
   else if (jumpto == [354]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pachriscarr.jpg";
   else if (jumpto == [355]) window.location = "http://www.newyankee.com/GetYankees2.cgi?padansigg.jpg";
   else if (jumpto == [356]) window.location = "http://www.newyankee.com/GetYankees2.cgi?padavecalabretta.jpg";
   else if (jumpto == [357]) window.location = "http://www.newyankee.com/GetYankees2.cgi?padennishoffman.jpg";
   else if (jumpto == [358]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pafrankschlipf.jpg";
   else if (jumpto == [359]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pajamesevanson.jpg";
   else if (jumpto == [360]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pajoekrol.jpg";
   else if (jumpto == [361]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pakatecrimmins.jpg";
   else if (jumpto == [362]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pamarshallkrebs.jpg";
   else if (jumpto == [363]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pascottsheaffer.jpg";
   else if (jumpto == [364]) window.location = "http://www.newyankee.com/GetYankees2.cgi?paterrycrippen.jpg";
   else if (jumpto == [365]) window.location = "http://www.newyankee.com/GetYankees2.cgi?patjpera.jpg";
   else if (jumpto == [366]) window.location = "http://www.newyankee.com/GetYankees2.cgi?patoddpatterson.jpg";
   else if (jumpto == [367]) window.location = "http://www.newyankee.com/GetYankees2.cgi?patomrehm.jpg";
   else if (jumpto == [368]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pavicschreck.jpg";
   else if (jumpto == [369]) window.location = "http://www.newyankee.com/GetYankees2.cgi?pawilliamhowen.jpg";
   else if (jumpto == [370]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ricarlruggieri.jpg";
   else if (jumpto == [371]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ripetermccrea.jpg";
   else if (jumpto == [372]) window.location = "http://www.newyankee.com/GetYankees2.cgi?scbillmovius.jpg";
   else if (jumpto == [373]) window.location = "http://www.newyankee.com/GetYankees2.cgi?scbryanrackley.jpg";
   else if (jumpto == [374]) window.location = "http://www.newyankee.com/GetYankees2.cgi?scchrisgoodman.jpg";
   else if (jumpto == [375]) window.location = "http://www.newyankee.com/GetYankees2.cgi?scdarrellmunn.jpg";
   else if (jumpto == [376]) window.location = "http://www.newyankee.com/GetYankees2.cgi?scdonsandusky.jpg";
   else if (jumpto == [377]) window.location = "http://www.newyankee.com/GetYankees2.cgi?scscotalexander.jpg";
   else if (jumpto == [378]) window.location = "http://www.newyankee.com/GetYankees2.cgi?sctimbajuscik.jpg";
   else if (jumpto == [379]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ststuartcoltart.jpg";
   else if (jumpto == [380]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnbilobautista.jpg";
   else if (jumpto == [381]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnbrucebowman.jpg";
   else if (jumpto == [382]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tndavidchipman.jpg";
   else if (jumpto == [383]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tndavidcizunas.jpg";
   else if (jumpto == [384]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tndavidreed.jpg";
   else if (jumpto == [385]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnhankdunkin.jpg";
   else if (jumpto == [386]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnkenwetherington.jpg";
   else if (jumpto == [387]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnrickgodboldt.jpg";
   else if (jumpto == [388]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnroyowen.jpg";
   else if (jumpto == [389]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnsteve.jpg";
   else if (jumpto == [390]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tntommymercks.jpg";
   else if (jumpto == [391]) window.location = "http://www.newyankee.com/GetYankees2.cgi?tnwarrenmonroe.jpg";
   else if (jumpto == [392]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txbillvanpelt.jpg";
   else if (jumpto == [393]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txcarolynmoncivais.jpg";
   else if (jumpto == [394]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txchucksteding.jpg";
   else if (jumpto == [395]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txclintlafont.jpg";
   else if (jumpto == [396]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txcurthackett.jpg";
   else if (jumpto == [397]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txdavidmcneill.jpg";
   else if (jumpto == [398]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txdonowen.jpg";
   else if (jumpto == [399]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txfrankcox.jpg";
   else if (jumpto == [400]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txglenbang.jpg";
   else if (jumpto == [401]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txhowardlaunius.jpg";
   else if (jumpto == [402]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txjamienorwood.jpg";
   else if (jumpto == [403]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txjimmarkle.jpg";
   else if (jumpto == [404]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txjimmcnamara.jpg";
   else if (jumpto == [405]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txjoelgulker.jpg";
   else if (jumpto == [406]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txjoeveillon.jpg";
   else if (jumpto == [407]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txjohnburns.jpg";
   else if (jumpto == [408]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txkeithmartin.jpg";
   else if (jumpto == [409]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txkennymiller.jpg";
   else if (jumpto == [410]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txkirkconstable.jpg";
   else if (jumpto == [411]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txkylekelley.jpg";
   else if (jumpto == [412]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txlesjones.jpg";
   else if (jumpto == [413]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txlynnlacey.jpg";
   else if (jumpto == [414]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txmarksimmons.jpg";
   else if (jumpto == [415]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txmauriceharris.jpg";
   else if (jumpto == [416]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txmichaelbrown.jpg";
   else if (jumpto == [417]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txrichardthomas.jpg";
   else if (jumpto == [418]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txrickent.jpg";
   else if (jumpto == [419]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txtomlovelace.jpg";
   else if (jumpto == [420]) window.location = "http://www.newyankee.com/GetYankees2.cgi?txvareckwalla.jpg";
   else if (jumpto == [421]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukbrianstainton.jpg";
   else if (jumpto == [422]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukdavegrimwood.jpg";
   else if (jumpto == [423]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukdavidevans.jpg";
   else if (jumpto == [424]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukgeoffbogg.jpg";
   else if (jumpto == [425]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukgordondale.jpg";
   else if (jumpto == [426]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukharborne.jpg";
   else if (jumpto == [427]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukjamesobrian.jpg";
   else if (jumpto == [428]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukjeffjones.jpg";
   else if (jumpto == [429]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukjohnworthington.jpg";
   else if (jumpto == [430]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukkeithrobinson.jpg";
   else if (jumpto == [431]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukkoojanzen.jpg";
   else if (jumpto == [432]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukleewebster.jpg";
   else if (jumpto == [433]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukpaultebbutt.jpg";
   else if (jumpto == [434]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukriaanstrydom.jpg";
   else if (jumpto == [435]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukrickdare.jpg";
   else if (jumpto == [436]) window.location = "http://www.newyankee.com/GetYankees2.cgi?ukterrychadwick.jpg";
   else if (jumpto == [437]) window.location = "http://www.newyankee.com/GetYankees2.cgi?utbobcanestrini.jpg";
   else if (jumpto == [438]) window.location = "http://www.newyankee.com/GetYankees2.cgi?utdonthornock.jpg";
   else if (jumpto == [439]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vaartgreen.jpg";
   else if (jumpto == [440]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vabobheller.jpg";
   else if (jumpto == [441]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vaclintadkins.jpg";
   else if (jumpto == [442]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vadanieltepe.jpg";
   else if (jumpto == [443]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vadanmeier.jpg";
   else if (jumpto == [444]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vadavidminnix.jpg";
   else if (jumpto == [445]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vadavidyoho.jpg";
   else if (jumpto == [446]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vadickthornsberry.jpg";
   else if (jumpto == [447]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vamarksimonds.jpg";
   else if (jumpto == [448]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vamichaelkoch.jpg";
   else if (jumpto == [449]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vamikeperozziello.jpg";
   else if (jumpto == [450]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vamikepingrey.jpg";
   else if (jumpto == [451]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vapatrickkearney.jpg";
   else if (jumpto == [452]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vapaulstreet.jpg";
   else if (jumpto == [453]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vatonydemasi.jpg";
   else if (jumpto == [454]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vatroylong.jpg";
   else if (jumpto == [455]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vatroylong2.jpg";
   else if (jumpto == [456]) window.location = "http://www.newyankee.com/GetYankees2.cgi?vaweslyon.jpg";
   else if (jumpto == [457]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wabryanthomas.jpg";
   else if (jumpto == [458]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wageorgebryan.jpg";
   else if (jumpto == [459]) window.location = "http://www.newyankee.com/GetYankees2.cgi?waglennpiersall.jpg";
   else if (jumpto == [460]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wajoewanjohi.jpg";
   else if (jumpto == [461]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wajohndrapala.jpg";
   else if (jumpto == [462]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wajohnfernstrom.jpg";
   else if (jumpto == [463]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wajohnmickelson.jpg";
   else if (jumpto == [464]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wakeithjohnson.jpg";
   else if (jumpto == [465]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wamarkdenman.jpg";
   else if (jumpto == [466]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wamiketaylor.jpg";
   else if (jumpto == [467]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wascottboyd.jpg";
   else if (jumpto == [468]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wibryanschappel.jpg";
   else if (jumpto == [469]) window.location = "http://www.newyankee.com/GetYankees2.cgi?widenniszuber.jpg";
   else if (jumpto == [470]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wigeorgebregar.jpg";
   else if (jumpto == [471]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wikevinwarren.jpg";
   else if (jumpto == [472]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wirichorde.jpg";
   else if (jumpto == [473]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wistevenricks.jpg";
   else if (jumpto == [474]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wiweswolfrom.jpg";
   else if (jumpto == [475]) window.location = "http://www.newyankee.com/GetYankees2.cgi?wvdannorby.jpg";
}





/*
 * This function comes from http://bugzilla.mozilla.org/show_bug.cgi?id=133897
 */
function validId(IDtext)
{
var res = "";

if(IDText.value==""){
                        print("You must enter a valid battery #")
                        return false
}
else if(IDText.value=="x522"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer_e2/energizer2.htm"
                        return true
}
else if(IDText.value=="x91"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer_e2/energizer2.htm"
                        return true
}
else if(IDText.value=="x92"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer_e2/energizer2.htm"
                        return true
}
else if(IDText.value=="x93"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer_e2/energizer2.htm"
                        return true
}
else if(IDText.value=="x95"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer_e2/energizer2.htm"
                        return true
}
else if(IDText.value=="521"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="522"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="528"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="529"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="539"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="e90"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="e91"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="e92"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="e93"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="e95"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_consumeroem.htm"
                        return true
                }
else if(IDText.value=="e96"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer_e2/energizer2.htm"
                        return true
                }
                else if(IDText.value=="en6"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en22"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en90"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en91"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en92"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en93"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en95"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en529"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en539"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="en715"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="edl4a"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="edl4as"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="edl4ac"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
                else if(IDText.value=="edl6a"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_industrial.htm"
                        return true
                }
else if(IDText.value=="3-0316"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }
else if(IDText.value=="3-0316i"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }
else if(IDText.value=="3-00411"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }
else if(IDText.value=="3-0411i"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-312"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }
else if(IDText.value=="3-312i"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-315"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-315i"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-315innc"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-315iwc"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-315wc"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-335"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-335i"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-335wc"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-335nnci"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-350"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-350i"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-3501wc"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-350wc"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-350nnci"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-361"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

else if(IDText.value=="3-361i"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/energizer/alkaline_oem_only.htm"
                        return true
                }

                else if(IDText.value=="a522"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/eveready/value.htm"
                        return true
                }
                else if(IDText.value=="a91"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/eveready/value.htm"
                        return true
                }
                else if(IDText.value=="a92"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/eveready/value.htm"
                        return true
                }
                else if(IDText.value=="a93"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/eveready/value.htm"
                        return true
                }
                else if(IDText.value=="a95"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/alkaline/eveready/value.htm"
                        return true
                }

else if(IDText.value=="510s"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_consumeroem.htm"
                        return true
                }

else if(IDText.value=="1209"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_consumeroem.htm"
                        return true
                }

else if(IDText.value=="1212"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_consumeroem.htm"
                        return true
                }

else if(IDText.value=="1215"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_consumeroem.htm"
                        return true
                }

else if(IDText.value=="1222"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_consumeroem.htm"
                        return true
                }

else if(IDText.value=="1235"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_consumeroem.htm"
                        return true
                }

else if(IDText.value=="1250"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_consumeroem.htm"
                        return true
                }
                else if(IDText.value=="206"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="246"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="266"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="276"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="411"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }


                else if(IDText.value=="412"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="413"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="415"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="416"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="455"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="467"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="489"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="493"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="497"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="504"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="505"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="711"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="732"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="763"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="ev190"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="ev115"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="ev122"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="ev131"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="ev135"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="ev150"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

                else if(IDText.value=="hs14196"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/carbon_zinc/carbon_zinc_industrial.htm"
                        return true
                }

else if(IDText.value=="2l76"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }
else if(IDText.value=="el1cr2"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }
else if(IDText.value=="crv3"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }

else if(IDText.value=="el2cr5"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }

else if(IDText.value=="el123ap"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }

else if(IDText.value=="el223ap"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }

else if(IDText.value=="l91"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }

else if(IDText.value=="l522"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }

else if(IDText.value=="l544"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithium.htm"
                        return true
                }

else if(IDText.value=="cr1025"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr1216"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr1220"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr1225"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr1616"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr1620"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr1632"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr2012"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr2016"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr2025"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr2032"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr2320"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr2430"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }

else if(IDText.value=="cr2450"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/lithium/lithiummin.htm"
                        return true
                }
                else if(IDText.value=="186"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="189"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="191"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="192"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="193"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="a23"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="a27"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="a76"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="a544"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="e11a"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

                else if(IDText.value=="e625g"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/manganese_dioxide/manganese_dioxide.htm"
                        return true
                }

else if(IDText.value=="301"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="303"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="309"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="315"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="317"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="319"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="321"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="329"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }
else if(IDText.value=="333"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }
else if(IDText.value=="335"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="337"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="339"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="341"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="344"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="346"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="350"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="357"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="361"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="362"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="364"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="365"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="366"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="370"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="371"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="373"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="376"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="377"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="379"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="381"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="384"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="386"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="387s"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="389"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="390"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="391"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="392"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="393"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="394"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="395"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="396"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="397"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

else if(IDText.value=="399"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }


else if(IDText.value=="epx76"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/silver/silver_oxide.htm"
                        return true
                }

                else if(IDText.value=="ac5"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/zinc_air/zinc_air.htm"
                        return true
                }

                else if(IDText.value=="ac10/230"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/zinc_air/zinc_air.htm"
                        return true
                }
                else if(IDText.value=="ac10"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/zinc_air/zinc_air.htm"
                        return true
                }
                else if(IDText.value=="ac13"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/zinc_air/zinc_air.htm"
                        return true
                }

                else if(IDText.value=="ac146x"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/zinc_air/zinc_air.htm"
                        return true
                }

                else if(IDText.value=="ac312"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/zinc_air/zinc_air.htm"
                        return true
                }

                else if(IDText.value=="ac675"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/zinc_air/zinc_air.htm"
                        return true
                }

else if(IDText.value=="chm24"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/accessories/rechargeableaccessories_chrger.htm"
                        return true
                }

else if(IDText.value=="chm4aa"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/accessories/rechargeableaccessories_chrger.htm"
                        return true
                }

else if(IDText.value=="chsm"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/accessories/rechargeableaccessories_chrger.htm"
                        return true
                }

else if(IDText.value=="chm4fc"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/accessories/rechargeableaccessories_chrger.htm"
                        return true
                }

else if(IDText.value=="cxl1000"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/accessories/rechargeableaccessories_chrger.htm"
                        return true
                }
                else if(IDText.value=="nh12"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_nimh.htm"
                        return true
                }
                else if(IDText.value=="nh15"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_nimh.htm"
                        return true
                }

                else if(IDText.value=="nh22"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_nimh.htm"
                        return true
                }

                else if(IDText.value=="nh35"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_nimh.htm"
                        return true
                }
                else if(IDText.value=="nh50"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_nimh.htm"
                        return true
                }


else if(IDText.value=="ccm5060"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="ccm5260"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="cm1060h"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="cm1360"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="cm2560"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="cm6136"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="cv3010"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="cv3012"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="cv3112"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="erc510"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc5160"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc520"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc525"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc530"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc545"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc560"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc570"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="erc580"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc590"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc600"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc610"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc620"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="erc630"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

else if(IDText.value=="erc640"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc650"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc660"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc670"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc680"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }
else if(IDText.value=="erc700"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscam.htm"
                        return true
                }

                else if(IDText.value=="cp2360"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }

                else if(IDText.value=="cp3036"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }


                else if(IDText.value=="cp3136"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }

                else if(IDText.value=="cp3336"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }

                else if(IDText.value=="cp5136"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }

                else if(IDText.value=="cp5648"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }

                else if(IDText.value=="cp5748"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }

                else if(IDText.value=="cp8049"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }

                else if(IDText.value=="cp8648"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }


                else if(IDText.value=="cpv5136"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }


                else if(IDText.value=="acp5036"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }


                else if(IDText.value=="acp5136"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }


                else if(IDText.value=="acp7160"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw120"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw210"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw220"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw230"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw240"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw305"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw310"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw320"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw400"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw500"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw510"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw520"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw530"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw600"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw610"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw700"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw720"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
                else if(IDText.value=="erw800"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscell.htm"
                        return true
                }
else if(IDText.value=="erp107"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp110"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp240"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp268"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp275"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp290"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp450"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp506"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp509"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp730"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erp9116"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="p2312"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p2322m"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="p2331"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p3201"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p3301"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p3302"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p3303"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p3306"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p3391"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p5256"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="p7300"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p7301"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="7302"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="7310"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p7320"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="p7330"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="p7340"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p7350"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }

else if(IDText.value=="p7360"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="p7400"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="p7501"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packscord.htm"
                        return true
                }
else if(IDText.value=="erd100"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packsdigicam.htm"
                        return true
                }
else if(IDText.value=="erd110"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packsdigicam.htm"
                        return true
                }
else if(IDText.value=="erd200"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packsdigicam.htm"
                        return true
                }
else if(IDText.value=="erd300"){
                        //Checks for id entry
                        res="../batteryinfo/product_offerings/rechargeable_consumer/rechargeable_consumer_packsdigicam.htm"
                        return true
                }













else if(IDText.value=="164"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="201"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="216"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="226"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="228"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="311"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="314"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }

else if(IDText.value=="313"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="323"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="325"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="333cz"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="343"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="354"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="355"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="387"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="388"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="417"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="420"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="457"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="460"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="477"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="479"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="482"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="484"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="487"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="490"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="491"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="496"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="509"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="510f"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="520"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="523"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="531"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="532"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="537"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="538"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="544"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="560"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="561"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="563"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="564"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="565"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="646"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="703"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="706"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="714"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="715"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="716"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="717"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="724"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="731"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="735"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="736"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="738"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="742"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="744"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="750"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="762s"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="773"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="778"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="781"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="812"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="815"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="835"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="850"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="904"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="912"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="915"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="935"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="950"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1015"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1035"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1050"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1150"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1231"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1461"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1463"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1562"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="1862"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="2356n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="2709n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="2744n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="2745n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="2746n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="2780n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ac41e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cc1096"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ccm1460"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ccm2460"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ccm4060a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ccm4060m"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cdc100"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ch12"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ch15"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ch2aa"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ch22"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ch35"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ch4"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ch50"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cm1060"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cm1560"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cm2360"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cm4160"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cm6036"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cm9072"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cm9172"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp2360"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp3336"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp3536"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp3736"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp5036"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp5160"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp5648"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp5960"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp6072"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp6172"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7049"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7072"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7148"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7149"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7160"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7172"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7248"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7261"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7348"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7548"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7661"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp7960"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8049"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8136"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8160"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8172"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8248"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8661"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8748"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }

else if(IDText.value=="cp8948"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp8960"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp9061"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp9148"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp9161"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cp9360"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs3336"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs5036"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs5460"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7048"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7072"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7148"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7149"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7160"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7248"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7261"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7348"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7448"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7548"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs7661"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs8136"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs8648"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs9061"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs9148"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cs9161"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cv2012"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cv2096"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cv3010s"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cv3012"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cv3060"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cv3112"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="cv3212"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e1"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e1n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e3"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e4"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e9"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e12"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e12n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e13e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e41e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e42"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e42n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e89"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e115"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e115n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e126"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e132"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e132n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e133"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e133n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e134"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e134n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e135"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e135n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e136"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e137"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e137n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e146x"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e152"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e163"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e164"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e164n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e165"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e169"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e177"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e233"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e233n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e235n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e236n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e286"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e289"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e312e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e340e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e400"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e400n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e401e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e401n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e450"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e502"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e601"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e625"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e630"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e640"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e640n"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e675e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302157"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302250"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302358"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302435"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302462"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302465"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302478"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302642"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302651"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302702"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302904"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302905"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e302908"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e303145"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e303236"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e303314"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e303394"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e303496"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="e303996"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ea6"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ea6f"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ea6ft"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ea6st"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en1a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en132a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en133a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en134a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en135a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en136a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en164a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en165a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en175a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en177a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="en640a"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ep175"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ep401e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ep675e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx1"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx4"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx13"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx14"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx23"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx25"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx27"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx29"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx30"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx625"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx640"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx675"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="epx825"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev6"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev9"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev10s"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev15"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev22"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev31"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev35"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev50"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev90"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="ev90hp"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="fcc2"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs6"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs10s"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs15"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs31"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs35"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs50"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs90"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs95"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs150"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="hs6571"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="if6"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="is6"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="is6t"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="p2321m"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="p2322"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="p2326m"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="p7307"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="p7507"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="qcc4"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="s13e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="s312e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="s41e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="s76e"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="t35"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="t50"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="w353"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/contents/discontinued_battery_index.htm"
                        return true
                }
else if(IDText.value=="3251"){
                        //Checks for id entry
                        res="../datasheets/flashlights/eveready.htm"
                        return true
                }
else if(IDText.value=="4212"){
                        //Checks for id entry
                        res="../datasheets/flashlights/eveready.htm"
                        return true
                }
else if(IDText.value=="4251"){
                        //Checks for id entry
                        res="../datasheets/flashlights/eveready.htm"
                        return true
                }
else if(IDText.value=="5109"){
                        //Checks for id entry
                        res="../datasheets/flashlights/eveready.htm"
                        return true
                }
else if(IDText.value=="2251"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="e220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="e250"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="e251"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="e251rc210"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="erg2c1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="glo4aa1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="rc220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="rc250"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="x112"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="x215"){
                        //Checks for id entry
                        res="../datasheets/flashlights/home.htm"
                        return true
                }
else if(IDText.value=="4215"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="5215"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="6212"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="bas24a"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="db24a1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="kcbg"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="kccl"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="kcdl"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="kcl2bu1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="kcwl"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="ltcr"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="lteb"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="ltpt"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="sl240"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="v220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/novelty.htm"
                        return true
                }
else if(IDText.value=="5100"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="8209"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="8215"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="9450"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="f101"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="f220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="f420"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="fab4dcm"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="fl450"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="k221"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="k251"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="led4aa1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="sp220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="tw420"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="tw450"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="wp220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="wp250"){
                        //Checks for id entry
                        res="../datasheets/flashlights/outdoor.htm"
                        return true
                }
else if(IDText.value=="cfl420"){
                        //Checks for id entry
                        res="../datasheets/flashlights/premium.htm"
                        return true
                }
else if(IDText.value=="d410"){
                        //Checks for id entry
                        res="../datasheets/flashlights/premium.htm"
                        return true
                }
else if(IDText.value=="d420"){
                        //Checks for id entry
                        res="../datasheets/flashlights/premium.htm"
                        return true
                }
else if(IDText.value=="fn450"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="in215"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="in251"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="in351"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="in421"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="k220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="k250"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="r215"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="r250"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="r450"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="tuf4d1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="v109"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="v115"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="v215"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="v250"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }
else if(IDText.value=="val2dl1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/work.htm"
                        return true
                }

else if(IDText.value=="459"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="208ind"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="231ind"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="1151"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="1251"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="1259"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="1351"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="1359"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="3251r"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="3251wh"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="4212wh"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="4250ind"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="5109ind"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="6212wh"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="9101ind"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="e250y"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="e251y"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="in220"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="in253"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="in420"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="in450"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="indwandr"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="indwandy"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="r215ind"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrial.htm"
                        return true
                }
else if(IDText.value=="pr2"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="pr3"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="pr4"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="pr6"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="pr7"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="pr12"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="pr13"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="pr35"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="112"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="222"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="243"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="258"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="407"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="425"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="1156"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="1651"){
                        //Checks for id entry
                        res="../datasheets/flashlights/standard.htm"
                        return true
                }
else if(IDText.value=="kpr102"){
                        //Checks for id entry
                        res="../datasheets/flashlights/krypton.htm"
                        return true
                }
else if(IDText.value=="kpr103"){
                        //Checks for id entry
                        res="../datasheets/flashlights/krypton.htm"
                        return true
                }
else if(IDText.value=="kpr104"){
                        //Checks for id entry
                        res="../datasheets/flashlights/krypton.htm"
                        return true
                }
else if(IDText.value=="kpr113"){
                        //Checks for id entry
                        res="../datasheets/flashlights/krypton.htm"
                        return true
                }
else if(IDText.value=="kpr116"){
                        //Checks for id entry
                        res="../datasheets/flashlights/krypton.htm"
                        return true
                }
else if(IDText.value=="kpr802"){
                        //Checks for id entry
                        res="../datasheets/flashlights/krypton.htm"
                        return true
                }
else if(IDText.value=="skpr823"){
                        //Checks for id entry
                        res="../datasheets/flashlights/krypton.htm"
                        return true
                }
else if(IDText.value=="hpr50"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenbulb.htm"
                        return true
                }
else if(IDText.value=="hpr51"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenbulb.htm"
                        return true
                }
else if(IDText.value=="hpr52"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenbulb.htm"
                        return true
                }
else if(IDText.value=="hpr53"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenbulb.htm"
                        return true
                }
else if(IDText.value=="f4t5"){
                        //Checks for id entry
                        res="../datasheets/flashlights/fluorescent.htm"
                        return true
                }
else if(IDText.value=="f6t5"){
                        //Checks for id entry
                        res="../datasheets/flashlights/fluorescent.htm"
                        return true
                }
else if(IDText.value=="t1-1"){
                        //Checks for id entry
                        res="../datasheets/flashlights/highintensity.htm"
                        return true
                }
else if(IDText.value=="t1-2"){
                        //Checks for id entry
                        res="../datasheets/flashlights/highintensity.htm"
                        return true
                }
else if(IDText.value=="t2-2"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenxenon.htm"
                        return true
                }
else if(IDText.value=="t2-3"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenxenon.htm"
                        return true
                }
else if(IDText.value=="t2-4"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenxenon.htm"
                        return true
                }
else if(IDText.value=="tx15-2"){
                        //Checks for id entry
                        res="../datasheets/flashlights/halogenxenon.htm"
                        return true
                }
else if(IDText.value=="4546ib"){
                        //Checks for id entry
                        res="../datasheets/flashlights/industrialbulb.htm"
                        return true
                }
else if(IDText.value=="LED"){
                        //Checks for id entry
                        res="../datasheets/flashlights/led.htm"
                        return true
                }



else if(IDText.value=="108"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="209"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="330"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="330y"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="331"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="331y"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="1251bk"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="2253"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="3233"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="3253"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="3415"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="3452"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="4220"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="4453"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="5154"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="5251"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="7369"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="8115"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="8415"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="b170"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="bkc1"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="d620"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="d820"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="e100"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="e252"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="e350"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="e420"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="em290"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="em420"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="f100"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="f215"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="f250"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="f415"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="h100"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="h250"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="h350"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="in25t"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="kcdb"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="kcsg"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="kctw"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="rc100"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="rc251"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="rc290"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="t430"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="v235"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="x250"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }
else if(IDText.value=="x350"){
                        //Checks for id entry
                        print("You have entered a Discontinued Product Number")
                        res="../datasheets/flashlights/discontinued_flashlight_index.htm"
                        return true
                }



else            {
                        print("You have entered an Invalid Product Number...Please try 'Select Product Group' search.")
                        return false
                }

}
