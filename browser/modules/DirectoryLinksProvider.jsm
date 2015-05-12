/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DirectoryLinksProvider"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const XMLHttpRequest =
  Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1", "nsIXMLHttpRequest");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => {
  return new TextDecoder();
});

// The filename where directory links are stored locally
const DIRECTORY_LINKS_FILE = "directoryLinks.json";
const DIRECTORY_LINKS_TYPE = "application/json";

// The preference that tells whether to match the OS locale
const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";

// The preference that tells what locale the user selected
const PREF_SELECTED_LOCALE = "general.useragent.locale";

// The preference that tells where to obtain directory links
const PREF_DIRECTORY_SOURCE = "browser.newtabpage.directory.source";

// The preference that tells where to send click/view pings
const PREF_DIRECTORY_PING = "browser.newtabpage.directory.ping";

// The preference that tells if newtab is enhanced
const PREF_NEWTAB_ENHANCED = "browser.newtabpage.enhanced";

// Only allow explicitly approved frecent sites with display name
const ALLOWED_FRECENT_SITES = new Map([
  [ 'airdroid.com,android-developers.blogspot.com,android.com,androidandme.com,androidapplications.com,androidapps.com,androidauthority.com,androidcentral.com,androidcommunity.com,androidfilehost.com,androidforums.com,androidguys.com,androidheadlines.com,androidpit.com,androidpolice.com,androidspin.com,androidtapp.com,androinica.com,droid-life.com,droidforums.net,droidviews.com,droidxforums.com,forum.xda-developers.com,phandroid.com,play.google.com,shopandroid.com,talkandroid.com,theandroidsoul.com,thedroidguy.com,videodroid.org',
    'Technology' ],
  [ 'assurancewireless.com,att.com,attsavings.com,boostmobile.com,budgetmobile.com,consumercellular.com,credomobile.com,gosmartmobile.com,h2owirelessnow.com,lycamobile.com,lycamobile.us,metropcs.com,myfamilymobile.com,polarmobile.com,qlinkwireless.com,republicwireless.com,sprint.com,straighttalk.com,t-mobile.com,tracfonewireless.com,verizonwireless.com,virginmobile.com,virginmobile.com.au,virginmobileusa.com,vodafone.co.uk,vodafone.com,vzwshop.com',
    'Mobile Phone' ],
  [ 'addons.mozilla.org,air.mozilla.org,blog.mozilla.org,bugzilla.mozilla.org,developer.mozilla.org,etherpad.mozilla.org,forums.mozillazine.org,hacks.mozilla.org,hg.mozilla.org,mozilla.org,planet.mozilla.org,quality.mozilla.org,support.mozilla.org,treeherder.mozilla.org,wiki.mozilla.org',
    'Mozilla' ],
  [ '3dprint.com,4sysops.com,access-programmers.co.uk,accountingweb.com,addictivetips.com,adweek.com,afterdawn.com,akihabaranews.com,anandtech.com,appsrumors.com,arstechnica.com,belkin.com,besttechinfo.com,betanews.com,bgr.com,botcrawl.com,breakingmuscle.com,canonrumors.com,cheap-phones.com,chip.de,chip.eu,cio.com,citeworld.com,cleanpcremove.com,cnet.com,commentcamarche.net,computer.org,computerhope.com,computershopper.com,computerweekly.com,contextures.com,coolest-gadgets.com,crn.com,csoonline.com,daniweb.com,data.com,datacenterknowledge.com,ddj.com,devicemag.com,digitaltrends.com,dottech.org,dpreview.com,dslreports.com,edugeek.net,eetimes.com,engadget.com,epic.com,eurekalert.org,eweek.com,experts-exchange.com,extremetech.com,fosshub.com,freesoftwaremagazine.com,funkyspacemonkey.com,futuremark.com,gadgetreview.com,ghacks.net,gizmodo.co.uk,gizmodo.com,globalsecurity.org,greenbot.com,gunup.com,guru3d.com,head-fi.org,hexus.net,hothardware.com,howtoforge.com,idg.com.au,idigitaltimes.com,idownloadblog.com,ihackmyi.com,ilounge.com,infomine.com,informationweek.com,intellireview.com,intomobile.com,iphonehacks.com,ismashphone.com,isource.com,it168.com,itechpost.com,itpro.co.uk,itworld.com,jailbreaknation.com,kioskea.net,laptoping.com,laptopmag.com,lightreading.com,livescience.com,malwaretips.com,mediaroom.com,mobilemag.com,modmyi.com,modmymobile.com,mophie.com,mozillazine.org,neoseeker.com,neowin.net,newscientist.com,newsoxy.com,nextadvisor.com,notebookcheck.com,notebookreview.com,nvidia.com,nwc.com,orafaq.com,osdir.com,osxdaily.com,our-hometown.com,pcadvisor.co.uk,pchome.net,pcmag.com,pconline.com.cn,pcpop.com,pcpro.co.uk,pcreview.co.uk,pcrisk.com,pcwelt.de,phonerebel.com,phonescoop.com,physorg.com,pocket-lint.com,post-theory.com,prnewswire.co.uk,prnewswire.com,programming4.us,quickpwn.com,readwrite.com,redmondpie.com,redorbit.com,reviewed.com,safer-networking.org,sciencedaily.com,sciencenews.org,scientificamerican.com,scientificblogging.com,sciverse.com,servicerow.com,sinfuliphone.com,singularityhub.com,slashdot.org,slashgear.com,softonic.com,softonic.com.br,softonic.fr,sophos.com,space.com,sparkfun.com,speedguide.net,stuff.tv,techdailynews.net,techdirt.com,techeblog.com,techhive.com,techie-buzz.com,technewsworld.com,techniqueworld.com,technobuffalo.com,technologyreview.com,technologytell.com,techpowerup.com,techpp.com,techrepublic.com,techshout.com,techweb.com,techworld.com,techworld.com.au,techworldtweets.com,telecomfile.com,tgdaily.com,theinquirer.net,thenextweb.com,theregister.co.uk,thermofisher.com,theverge.com,thewindowsclub.com,tomsguide.com,tomshardware.com,tomsitpro.com,toptenreviews.com,trustedreviews.com,tuaw.com,tweaktown.com,ubergizmo.com,unwiredview.com,venturebeat.com,wccftech.com,webmonkey.com,webpronews.com,windows7codecs.com,windowscentral.com,windowsitpro.com,windowstechies.com,winsupersite.com,wired.co.uk,wired.com,wp-themes.com,xda-developers.com,xml.com,zdnet.com,zmescience.com,zol.com.cn',
    'Technology' ],
  [ '9to5mac.com,appadvice.com,apple.com,appleinsider.com,appleturns.com,appsafari.com,cultofmac.com,everymac.com,insanelymac.com,iphoneunlockspot.com,isource.com,itunes.apple.com,lowendmac.com,mac-forums.com,macdailynews.com,macenstein.com,macgasm.net,macintouch.com,maclife.com,macnews.com,macnn.com,macobserver.com,macosx.com,macpaw.com,macrumors.com,macsales.com,macstories.net,macupdate.com,macuser.co.uk,macworld.co.uk,macworld.com,maxiapple.com,spymac.com,theapplelounge.com',
    'Technology' ],
  [ 'alistapart.com,answers.microsoft.com,backpack.openbadges.org,blog.chromium.org,caniuse.com,codefirefox.com,codepen.io,css-tricks.com,css3generator.com,cssdeck.com,csswizardry.com,devdocs.io,docs.angularjs.org,ghacks.net,github.com,html5demos.com,html5rocks.com,html5test.com,iojs.org,khanacademy.org,l10n.mozilla.org,learn.jquery.com,marketplace.firefox.com,mozilla-hispano.org,mozillians.org,news.ycombinator.com,npmjs.com,packagecontrol.io,quirksmode.org,readwrite.com,reps.mozilla.org,smashingmagazine.com,stackoverflow.com,status.modern.ie,teamtreehouse.com,tutorialspoint.com,udacity.com,validator.w3.org,w3.org,w3cschool.cc,w3schools.com,whatcanidoformozilla.org',
    'Web Development' ],
  [ 'classroom.google.com,codecademy.com,elearning.ut.ac.id,khanacademy.org,learn.jquery.com,teamtreehouse.com,tutorialspoint.com,udacity.com,w3cschool.cc,w3schools.com',
    'Web Education' ],
  [ 'abebooks.co.uk,abebooks.com,alibris.com,allaboutcircuits.com,allyoucanbooks.com,answersingenesis.org,artnet.com,audiobooks.com,barnesandnoble.com,barnesandnobleinc.com,bartleby.com,betterworldbooks.com,biggerbooks.com,bncollege.com,bookbyte.com,bookdepository.com,bookfinder.com,bookrenter.com,booksamillion.com,booksite.com,boundless.com,brookstone.com,btol.com,calibre-ebook.com,campusbookrentals.com,casadellibro.com,cbomc.com,cengagebrain.com,chapters.indigo.ca,christianbook.com,ciscopress.com,coursesmart.com,cqpress.com,crafterschoice.com,crossings.com,cshlp.org,deseretbook.com,directtextbook.com,discountmags.com,doubledaybookclub.com,doubledaylargeprint.com,doverpublications.com,ebooks.com,ecampus.com,fellabooks.net,fictionwise.com,flatworldknowledge.com,grolier.com,harpercollins.com,hayhouse.com,historybookclub.com,hpb.com,hpbmarketplace.com,interweave.com,iseeme.com,katiekazoo.com,knetbooks.com,learnoutloud.com,librarything.com,literaryguild.com,lulu.com,lww.com,macmillan.com,magazines.com,mbsdirect.net,militarybookclub.com,mypearsonstore.com,mysteryguild.com,netplaces.com,noble.com,novelguide.com,onespirit.com,oxfordjournals.org,paperbackswap.com,papy.co.jp,peachpit.com,penguin.com,penguingroup.com,pimsleur.com,powells.com,qpb.com,quepublishing.com,reviews.com,rhapsodybookclub.com,rodalestore.com,royalsocietypublishing.org,sagepub.com,scrubsmag.com,sfbc.com,simonandschuster.com,simonandschuster.net,simpletruths.com,teach12.net,textbooks.com,textbookx.com,thegoodcook.com,thriftbooks.com,tlsbooks.com,toshibabookplace.com,tumblebooks.com,urbookdownload.com,valorebooks.com,valuemags.com,wwnorton.com,zoobooks.com',
    'Literature' ],
  [ 'aceshowbiz.com,aintitcoolnews.com,askkissy.com,askmen.com,atraf.co.il,audioboom.com,beamly.com,blippitt.com,bollywoodlife.com,bossip.com,buzzlamp.com,celebdirtylaundry.com,celebfocus.com,celebitchy.com,celebrity-gossip.net,celebrityabout.com,celebwild.com,chisms.net,concertboom.com,crushable.com,cultbox.co.uk,dailyentertainmentnews.com,dayscafe.com,deadline.com,deathandtaxesmag.com,diaryofahollywoodstreetking.com,digitalspy.com,egotastic.com,empirenews.net,enelbrasero.com,everydaycelebs.com,ew.com,extratv.com,facade.com,fanaru.com,fhm.com,geektyrant.com,glamourpage.com,heatworld.com,hlntv.com,hollyscoop.com,hollywoodreporter.com,hollywoodtuna.com,hypable.com,infotransfer.net,insideedition.com,interaksyon.com,jezebel.com,justjared.com,justjaredjr.com,komando.com,koreaboo.com,maxgo.com,maxim.com,maxviral.com,mediatakeout.com,mosthappy.com,moviestalk.com,my.ology.com,ngoisao.net,nofilmschool.com,nolocreo.com,octane.tv,ouchpress.com,people.com,peopleenespanol.com,perezhilton.com,pinkisthenewblog.com,platotv.tv,playbill.com,playbillvault.com,playgroundmag.net,popeater.com,popnhop.com,popsugar.co.uk,popsugar.com,purepeople.com,radaronline.com,rantchic.com,reshareworthy.com,rinkworks.com,ripbird.com,sara-freder.com,screenjunkies.com,soapcentral.com,soapoperadigest.com,sobadsogood.com,splitsider.com,starcasm.net,starpulse.com,straightfromthea.com,stupidcelebrities.net,stupiddope.com,tbn.org,theawesomedaily.com,theawl.com,thefrisky.com,thefw.com,theresacaputo.com,thezooom.com,tvnotas.com.mx,twanatells.com,vanswarpedtour.com,vietgiaitri.com,viral.buzz,vulture.com,wakavision.com,worthytales.net,wwtdd.com,younghollywood.com',
    'Entertainment News' ],
  [ '247wallst.com,4-traders.com,advfn.com,agweb.com,allbusiness.com,barchart.com,barrons.com,beckershospitalreview.com,benzinga.com,bizjournals.com,bizsugar.com,bloomberg.com,bloomberglaw.com,business-standard.com,businessinsider.com,businessinsider.com.au,businesspundit.com,businessweek.com,businesswire.com,cboe.com,cheatsheet.com,chicagobusiness.com,cjonline.com,cnbc.com,cnnmoney.com,cqrcengage.com,dailyfinance.com,dailyfx.com,dealbreaker.com,djindexes.com,dowjones.com,easierstreetdaily.com,economist.com,economyandmarkets.com,economywatch.com,edweek.org,eleconomista.es,entrepreneur.com,etfdailynews.com,etfdb.com,ewallstreeter.com,fastcolabs.com,fastcompany.com,financeformulas.net,financialpost.com,flife.de,forbes.com,forexpros.com,fortune.com,foxbusiness.com,ft.com,ftpress.com,fx-exchange.com,hbr.org,howdofinance.com,ibtimes.com,inc.com,investopedia.com,investors.com,investorwords.com,journalofaccountancy.com,kiplinger.com,lendingandcredit.net,lfb.org,mainstreet.com,markettraders.com,marketwatch.com,maxkeiser.com,minyanville.com,ml.com,moneycontrol.com,moneymappress.com,moneynews.com,moneysavingexpert.com,morningstar.com,mortgagenewsdaily.com,motleyfool.com,mt.co.kr,nber.org,nyse.com,oilprice.com,pewsocialtrends.org,principal.com,qz.com,rantfinance.com,realclearmarkets.com,recode.net,reuters.ca,reuters.co.in,reuters.co.uk,reuters.com,rttnews.com,seekingalpha.com,smallbiztrends.com,streetinsider.com,thecheapinvestor.com,theeconomiccollapseblog.com,themoneyconverter.com,thestreet.com,tickertech.com,tradingeconomics.com,updown.com,valuewalk.com,wikinvest.com,wsj.com,zacks.com',
    'Financial News' ],
  [ '10tv.com,8newsnow.com,9news.com,abc.net.au,abc7.com,abc7chicago.com,abcnews.go.com,aclu.org,activistpost.com,ajc.com,al.com,alan.com,alarab.net,aljazeera.com,americanthinker.com,app.com,aristeguinoticias.com,azcentral.com,baltimoresun.com,becomingminimalist.com,beforeitsnews.com,bigstory.ap.org,blackamericaweb.com,bloomberg.com,bloombergview.com,boston.com,bostonherald.com,breitbart.com,buffalonews.com,c-span.org,canada.com,cbs46.com,cbsnews.com,chicagotribune.com,chron.com,citizensvoice.com,citylab.com,cleveland.com,cnn.com,coed.com,countercurrentnews.com,courant.com,ctvnews.ca,dailyherald.com,dailynews.com,dallasnews.com,delawareonline.com,democratandchronicle.com,democraticunderground.com,democrats.org,denverpost.com,desmoinesregister.com,dispatch.com,elcomercio.pe,english.aljazeera.net,examiner.com,farsnews.com,firstcoastnews.com,firstpost.com,firsttoknow.com,foreignpolicy.com,foxnews.com,freebeacon.com,freep.com,fresnobee.com,gazette.com,global.nytimes.com,heraldtribune.com,hindustantimes.com,hngn.com,humanevents.com,huzlers.com,indiatimes.com,indystar.com,irishtimes.com,jacksonville.com,jpost.com,jsonline.com,kansascity.com,kctv5.com,kentucky.com,kickerdaily.com,king5.com,kmov.com,knoxnews.com,kpho.com,kvue.com,kwqc.com,kxan.com,lainformacion.com,latimes.com,ldnews.com,lex18.com,linternaute.com,livemint.com,lostateminor.com,m24.ru,macleans.ca,manchestereveningnews.co.uk,marinecorpstimes.com,masslive.com,mavikocaeli.com.tr,mcall.com,medium.com,mentalfloss.com,mercurynews.com,metro.us,miamiherald.com,militarytimes.com,mk.ru,mlive.com,mondotimes.com,montrealgazette.com,msnbc.com,msnewsnow.com,mynews13.com,mysanantonio.com,mysuncoast.com,nbclosangeles.com,nbcnewyork.com,nbcphiladelphia.com,ndtv.com,newindianexpress.com,news.cincinnati.com,news.google.com,news.msn.com,news.yahoo.com,news10.net,news8000.com,newsday.com,newsdaymarketing.net,newsen.com,newsmax.com,newsobserver.com,newsok.com,newsru.ua,newstatesman.com,newszoom.com,nj.com,nola.com,northjersey.com,nouvelobs.com,npr.org,nwfdailynews.com,nwitimes.com,nydailynews.com,nytimes.com,observer.com,ocregister.com,okcfox.com,omaha.com,onenewspage.com,ontheissues.org,oregonlive.com,orlandosentinel.com,palmbeachpost.com,pe.com,pennlive.com,philly.com,pilotonline.com,polar.com,post-gazette.com,postandcourier.com,presstelegram.com,presstv.ir,propublica.org,providencejournal.com,realclearpolitics.com,recorderonline.com,reporterdock.com,reporterherald.com,respublica.al,reuters.com,rg.ru,roanoke.com,sacbee.com,scmp.com,scnow.com,sdpnoticias.com,seattletimes.com,semana.com,sfgate.com,sharepowered.com,sinembargo.mx,slate.com,sltrib.com,sotomayortv.com,sourcewatch.org,spectator.co.uk,squaremirror.com,star-telegram.com,staradvertiser.com,startribune.com,statesman.com,stltoday.com,streetwise.co,stuff.co.nz,success.com,suffolknewsherald.com,sun-sentinel.com,sunnewsnetwork.ca,suntimes.com,supernewschannel.com,surenews.com,svoboda.org,syracuse.com,tampabay.com,tbd.com,telegram.com,telegraph.co.uk,tennessean.com,the-open-mind.com,theadvocate.com,theage.com.au,theatlantic.com,thebarefootwriter.com,theblaze.com,thecalifornian.com,thedailysheeple.com,thefix.com,theintelligencer.net,thelocal.com,thenational.ae,thenewstribune.com,theparisreview.org,thereporter.com,therepublic.com,thestar.com,thetelegram.com,thetimes.co.uk,theuspatriot.com,time.com,timescall.com,timesdispatch.com,timesleaderonline.com,timesofisrael.com,toledoblade.com,toprightnews.com,townhall.com,tpnn.com,trendolizer.com,triblive.com,tribune.com.pk,tricities.com,troymessenger.com,trueactivist.com,truthandaction.org,tsn.ua,tulsaworld.com,twincities.com,upi.com,usatoday.com,utsandiego.com,vagazette.com,viralwomen.com,vitalworldnews.com,voasomali.com,vox.com,washingtonexaminer.com,washingtonpost.com,watchdog.org,wave3.com,wavy.com,wbay.com,wbtw.com,wcpo.com,wctrib.com,wdtn.com,weeklystandard.com,westernjournalism.com,wfsb.com,wgrz.com,whas11.com,winonadailynews.com,wishtv.com,wistv.com,wkbn.com,wkow.com,wlfi.com,wmtw.com,wmur.com,wopular.com,world-top-news.com,worldnews.com,wplol.us,wpsdlocal6.com,wptz.com,wric.com,wsmv.com,wthitv.com,wthr.com,wtnh.com,wtol.com,wtsp.com,wvec.com,wwlp.com,wwltv.com,wyff4.com,yonhapnews.co.kr,yourbreakingnews.com',
    'News' ],
  [ '2k.com,360game.vn,4399.com,a10.com,activision.com,addictinggames.com,alawar.com,alienwarearena.com,anagrammer.com,andkon.com,aq.com,arcadeprehacks.com,arcadeyum.com,arcgames.com,archeagegame.com,armorgames.com,askmrrobot.com,battle.net,battlefieldheroes.com,bigfishgames.com,bigpoint.com,bioware.com,bluesnews.com,boardgamegeek.com,bollyheaven.com,bubblebox.com,bukkit.org,bungie.net,buycraft.net,callofduty.com,candystand.com,cda.pl,challonge.com,championselect.net,cheapassgamer.com,cheatcc.com,cheatengine.org,cheathappens.com,chess.com,civfanatics.com,clashofclans-tools.com,clashofclansbuilder.com,comdotgame.com,commonsensemedia.org,coolrom.com,crazygames.com,csgolounge.com,curse.com,d20pfsrd.com,destructoid.com,diablofans.com,diablowiki.net,didigames.com,dota2.com,dota2lounge.com,dressupgames.com,dulfy.net,ebog.com,elderscrollsonline.com,elitedangerous.com,elitepvpers.com,emuparadise.me,enjoydressup.com,escapegames24.com,escapistmagazine.com,eventhubs.com,eveonline.com,farming-simulator.com,feed-the-beast.com,flashgames247.com,flightrising.com,flipline.com,flonga.com,freegames.ws,freeonlinegames.com,fresh-hotel.org,friv.com,friv.today,fullypcgames.net,funny-games.biz,funtrivia.com,futhead.com,g2a.com,gamasutra.com,game-debate.com,game-oldies.com,game321.com,gamebaby.com,gamebaby.net,gamebanana.com,gamefaqs.com,gamefly.com,gamefront.com,gamegape.com,gamehouse.com,gameinformer.com,gamejolt.com,gamemazing.com,gamemeteor.com,gamerankings.com,gamersgate.com,games-msn.com,games-workshop.com,games.com,games2girls.com,gamesbox.com,gamesfreak.net,gametop.com,gametracker.com,gametrailers.com,gamezhero.com,gbatemp.net,geforce.com,gematsu.com,giantbomb.com,girl.me,girlsgames123.com,girlsplay.com,gog.com,gogames.me,gonintendo.com,goodgamestudios.com,gosugamers.net,greenmangaming.com,gtaforums.com,gtainside.com,guildwars2.com,hackedarcadegames.com,hearthpwn.com,hirezstudios.com,hitbox.tv,hltv.org,howrse.com,icy-veins.com,indiedb.com,jayisgames.com,jigzone.com,joystiq.com,juegosdechicas.com,kabam.com,kbhgames.com,kerbalspaceprogram.com,king.com,kixeye.com,kizi.com,kogama.com,kongregate.com,kotaku.com,lolcounter.com,lolking.net,lolnexus.com,lolpro.com,lolskill.net,lootcrate.com,lumosity.com,mafa.com,mangafox.me,mangapark.com,mariowiki.com,maxgames.com,megagames.com,metacritic.com,mindjolt.com,minecraft.net,minecraftforum.net,minecraftservers.org,minecraftskins.com,mineplex.com,miniclip.com,mmo-champion.com,mmobomb.com,mmohuts.com,mmorpg.com,mmosite.com,mobafire.com,moddb.com,modxvm.com,mojang.com,moshimonsters.com,mousebreaker.com,moviestarplanet.com,mtgsalvation.com,muchgames.com,myonlinearcade.com,myplaycity.com,myrealgames.com,mythicspoiler.com,n4g.com,newgrounds.com,nexon.net,nexusmods.com,ninjakiwi.com,nintendo.com,nintendoeverything.com,nintendolife.com,nitrome.com,nosteam.ro,notdoppler.com,noxxic.com,operationsports.com,origin.com,ownedcore.com,pacogames.com,pathofexile.com,pcgamer.com,pch.com,pcsx2.net,penny-arcade.com,planetminecraft.com,plarium.com,playdota.com,playpink.com,playsides.com,playstationlifestyle.net,playstationtrophies.org,pog.com,pokemon.com,polygon.com,popcap.com,primarygames.com,probuilds.net,ps3hax.net,psnprofiles.com,psu.com,qq.com,r2games.com,resourcepack.net,retrogamer.com,rewardtv.com,riotgames.com,robertsspaceindustries.com,roblox.com,robocraftgame.com,rockpapershotgun.com,rockstargames.com,roosterteeth.com,runescape.com,schoolofdragons.com,screwattack.com,scufgaming.com,segmentnext.com,shacknews.com,shockwave.com,shoryuken.com,siliconera.com,silvergames.com,skydaz.com,smashbros.com,solomid.net,starcitygames.com,starsue.net,steamcommunity.com,steamgifts.com,strategywiki.org,supercheats.com,surrenderat20.net,swtor.com,tankionline.com,tcgplayer.com,teamfortress.com,teamliquid.net,tetrisfriends.com,thesims3.com,thesimsresource.com,thetechgame.com,topg.org,totaljerkface.com,toucharcade.com,transformice.com,trueachievements.com,twcenter.net,twitch.tv,twoplayergames.org,unity3d.com,vg247.com,vgchartz.com,videogamesblogger.com,warframe.com,warlight.net,warthunder.com,watchcartoononline.com,websudoku.com,wildstar-online.com,wildtangent.com,wineverygame.com,wizards.com,worldofsolitaire.com,worldoftanks.com,wowhead.com,wowprogress.com,wowwiki.com,xbox.com,xbox360iso.com,xboxachievements.com,xfire.com,xtremetop100.com,y8.com,yoyogames.com,zybez.net,zynga.com',
    'Video Game' ],
]);

// Only allow link urls that are http(s)
const ALLOWED_LINK_SCHEMES = new Set(["http", "https"]);

// Only allow link image urls that are https or data
const ALLOWED_IMAGE_SCHEMES = new Set(["https", "data"]);

// The frecency of a directory link
const DIRECTORY_FRECENCY = 1000;

// The frecency of a suggested link
const SUGGESTED_FRECENCY = Infinity;

// The filename where frequency cap data stored locally
const FREQUENCY_CAP_FILE = "frequencyCap.json";

// Default settings for daily and total frequency caps
const DEFAULT_DAILY_FREQUENCY_CAP = 3;
const DEFAULT_TOTAL_FREQUENCY_CAP = 10;

// Default timeDelta to prune unused frequency cap objects
// currently set to 10 days in milliseconds
const DEFAULT_PRUNE_TIME_DELTA = 10*24*60*60*1000;

// The min number of visible (not blocked) history tiles to have before showing suggested tiles
const MIN_VISIBLE_HISTORY_TILES = 8;

// Divide frecency by this amount for pings
const PING_SCORE_DIVISOR = 10000;

// Allowed ping actions remotely stored as columns: case-insensitive [a-z0-9_]
const PING_ACTIONS = ["block", "click", "pin", "sponsored", "sponsored_link", "unpin", "view"];

/**
 * Singleton that serves as the provider of directory links.
 * Directory links are a hard-coded set of links shown if a user's link
 * inventory is empty.
 */
let DirectoryLinksProvider = {

  __linksURL: null,

  _observers: new Set(),

  // links download deferred, resolved upon download completion
  _downloadDeferred: null,

  // download default interval is 24 hours in milliseconds
  _downloadIntervalMS: 86400000,

  /**
   * A mapping from eTLD+1 to an enhanced link objects
   */
  _enhancedLinks: new Map(),

  /**
   * A mapping from site to a list of suggested link objects
   */
  _suggestedLinks: new Map(),

  /**
   * Frequency Cap object - maintains daily and total tile counts, and frequency cap settings
   */
  _frequencyCaps: {},

  /**
   * A set of top sites that we can provide suggested links for
   */
  _topSitesWithSuggestedLinks: new Set(),

  get _observedPrefs() Object.freeze({
    enhanced: PREF_NEWTAB_ENHANCED,
    linksURL: PREF_DIRECTORY_SOURCE,
    matchOSLocale: PREF_MATCH_OS_LOCALE,
    prefSelectedLocale: PREF_SELECTED_LOCALE,
  }),

  get _linksURL() {
    if (!this.__linksURL) {
      try {
        this.__linksURL = Services.prefs.getCharPref(this._observedPrefs["linksURL"]);
      }
      catch (e) {
        Cu.reportError("Error fetching directory links url from prefs: " + e);
      }
    }
    return this.__linksURL;
  },

  /**
   * Gets the currently selected locale for display.
   * @return  the selected locale or "en-US" if none is selected
   */
  get locale() {
    let matchOS;
    try {
      matchOS = Services.prefs.getBoolPref(PREF_MATCH_OS_LOCALE);
    }
    catch (e) {}

    if (matchOS) {
      return Services.locale.getLocaleComponentForUserAgent();
    }

    try {
      let locale = Services.prefs.getComplexValue(PREF_SELECTED_LOCALE,
                                                  Ci.nsIPrefLocalizedString);
      if (locale) {
        return locale.data;
      }
    }
    catch (e) {}

    try {
      return Services.prefs.getCharPref(PREF_SELECTED_LOCALE);
    }
    catch (e) {}

    return "en-US";
  },

  /**
   * Set appropriate default ping behavior controlled by enhanced pref
   */
  _setDefaultEnhanced: function DirectoryLinksProvider_setDefaultEnhanced() {
    if (!Services.prefs.prefHasUserValue(PREF_NEWTAB_ENHANCED)) {
      let enhanced = true;
      try {
        // Default to not enhanced if DNT is set to tell websites to not track
        if (Services.prefs.getBoolPref("privacy.donottrackheader.enabled")) {
          enhanced = false;
        }
      }
      catch(ex) {}
      Services.prefs.setBoolPref(PREF_NEWTAB_ENHANCED, enhanced);
    }
  },

  observe: function DirectoryLinksProvider_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed") {
      switch (aData) {
        // Re-set the default in case the user clears the pref
        case this._observedPrefs.enhanced:
          this._setDefaultEnhanced();
          break;

        case this._observedPrefs.linksURL:
          delete this.__linksURL;
          // fallthrough

        // Force directory download on changes to fetch related prefs
        case this._observedPrefs.matchOSLocale:
        case this._observedPrefs.prefSelectedLocale:
          this._fetchAndCacheLinksIfNecessary(true);
          break;
      }
    }
  },

  _addPrefsObserver: function DirectoryLinksProvider_addObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.addObserver(prefName, this, false);
    }
  },

  _removePrefsObserver: function DirectoryLinksProvider_removeObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.removeObserver(prefName, this);
    }
  },

  _cacheSuggestedLinks: function(link) {
    if (!link.frecent_sites || "sponsored" == link.type) {
      // Don't cache links that don't have the expected 'frecent_sites' or are sponsored.
      return;
    }
    for (let suggestedSite of link.frecent_sites) {
      let suggestedMap = this._suggestedLinks.get(suggestedSite) || new Map();
      suggestedMap.set(link.url, link);
      this._setupStartEndTime(link);
      this._suggestedLinks.set(suggestedSite, suggestedMap);
    }
  },

  _fetchAndCacheLinks: function DirectoryLinksProvider_fetchAndCacheLinks(uri) {
    // Replace with the same display locale used for selecting links data
    uri = uri.replace("%LOCALE%", this.locale);
    uri = uri.replace("%CHANNEL%", UpdateChannel.get());

    let deferred = Promise.defer();
    let xmlHttp = new XMLHttpRequest();

    let self = this;
    xmlHttp.onload = function(aResponse) {
      let json = this.responseText;
      if (this.status && this.status != 200) {
        json = "{}";
      }
      OS.File.writeAtomic(self._directoryFilePath, json, {tmpPath: self._directoryFilePath + ".tmp"})
        .then(() => {
          deferred.resolve();
        },
        () => {
          deferred.reject("Error writing uri data in profD.");
        });
    };

    xmlHttp.onerror = function(e) {
      deferred.reject("Fetching " + uri + " results in error code: " + e.target.status);
    };

    try {
      xmlHttp.open("GET", uri);
      // Override the type so XHR doesn't complain about not well-formed XML
      xmlHttp.overrideMimeType(DIRECTORY_LINKS_TYPE);
      // Set the appropriate request type for servers that require correct types
      xmlHttp.setRequestHeader("Content-Type", DIRECTORY_LINKS_TYPE);
      xmlHttp.send();
    } catch (e) {
      deferred.reject("Error fetching " + uri);
      Cu.reportError(e);
    }
    return deferred.promise;
  },

  /**
   * Downloads directory links if needed
   * @return promise resolved immediately if no download needed, or upon completion
   */
  _fetchAndCacheLinksIfNecessary: function DirectoryLinksProvider_fetchAndCacheLinksIfNecessary(forceDownload=false) {
    if (this._downloadDeferred) {
      // fetching links already - just return the promise
      return this._downloadDeferred.promise;
    }

    if (forceDownload || this._needsDownload) {
      this._downloadDeferred = Promise.defer();
      this._fetchAndCacheLinks(this._linksURL).then(() => {
        // the new file was successfully downloaded and cached, so update a timestamp
        this._lastDownloadMS = Date.now();
        this._downloadDeferred.resolve();
        this._downloadDeferred = null;
        this._callObservers("onManyLinksChanged")
      },
      error => {
        this._downloadDeferred.resolve();
        this._downloadDeferred = null;
        this._callObservers("onDownloadFail");
      });
      return this._downloadDeferred.promise;
    }

    // download is not needed
    return Promise.resolve();
  },

  /**
   * @return true if download is needed, false otherwise
   */
  get _needsDownload () {
    // fail if last download occured less then 24 hours ago
    if ((Date.now() - this._lastDownloadMS) > this._downloadIntervalMS) {
      return true;
    }
    return false;
  },

  /**
   * Reads directory links file and parses its content
   * @return a promise resolved to an object with keys 'directory' and 'suggested',
   *         each containing a valid list of links,
   *         or {'directory': [], 'suggested': []} if read or parse fails.
   */
  _readDirectoryLinksFile: function DirectoryLinksProvider_readDirectoryLinksFile() {
    let emptyOutput = {directory: [], suggested: [], enhanced: []};
    return OS.File.read(this._directoryFilePath).then(binaryData => {
      let output;
      try {
        let json = gTextDecoder.decode(binaryData);
        let linksObj = JSON.parse(json);
        output = {directory: linksObj.directory || [],
                  suggested: linksObj.suggested || [],
                  enhanced:  linksObj.enhanced  || []};
      }
      catch (e) {
        Cu.reportError(e);
      }
      return output || emptyOutput;
    },
    error => {
      Cu.reportError(error);
      return emptyOutput;
    });
  },

  /**
   * Translates link.time_limits to UTC miliseconds and sets
   * link.startTime and link.endTime properties in link object
   */
  _setupStartEndTime: function DirectoryLinksProvider_setupStartEndTime(link) {
    // set start/end limits. Use ISO_8601 format: '2014-01-10T20:20:20.600Z'
    // (details here http://en.wikipedia.org/wiki/ISO_8601)
    // Note that if timezone is missing, FX will interpret as local time
    // meaning that the server can sepecify any time, but if the capmaign
    // needs to start at same time across multiple timezones, the server
    // omits timezone indicator
    if (!link.time_limits) {
      return;
    }

    let parsedTime;
    if (link.time_limits.start) {
      parsedTime = Date.parse(link.time_limits.start);
      if (parsedTime && !isNaN(parsedTime)) {
        link.startTime = parsedTime;
      }
    }
    if (link.time_limits.end) {
      parsedTime = Date.parse(link.time_limits.end);
      if (parsedTime && !isNaN(parsedTime)) {
        link.endTime = parsedTime;
      }
    }
  },

  /*
   * Handles campaign timeout
   */
  _onCampaignTimeout: function DirectoryLinksProvider_onCampaignTimeout() {
    // _campaignTimeoutID is invalid here, so just set it to null
    this._campaignTimeoutID = null;
    this._updateSuggestedTile();
  },

  /*
   * Clears capmpaign timeout
   */
  _clearCampaignTimeout: function DirectoryLinksProvider_clearCampaignTimeout() {
    if (this._campaignTimeoutID) {
      clearTimeout(this._campaignTimeoutID);
      this._campaignTimeoutID = null;
    }
  },

  /**
   * Setup capmpaign timeout to recompute suggested tiles upon
   * reaching soonest start or end time for the campaign
   * @param timeout in milliseconds
   */
  _setupCampaignTimeCheck: function DirectoryLinksProvider_setupCampaignTimeCheck(timeout) {
    // sanity check
    if (!timeout || timeout <= 0) {
      return;
    }
    this._clearCampaignTimeout();
    // setup next timeout
    this._campaignTimeoutID = setTimeout(this._onCampaignTimeout.bind(this), timeout);
  },

  /**
   * Test link for campaign time limits: checks if link falls within start/end time
   * and returns an object containing a use flag and the timeoutDate milliseconds
   * when the link has to be re-checked for campaign start-ready or end-reach
   * @param link
   * @return object {use: true or false, timeoutDate: milliseconds or null}
   */
  _testLinkForCampaignTimeLimits: function DirectoryLinksProvider_testLinkForCampaignTimeLimits(link) {
    let currentTime = Date.now();
    // test for start time first
    if (link.startTime && link.startTime > currentTime) {
      // not yet ready for start
      return {use: false, timeoutDate: link.startTime};
    }
    // otherwise check for end time
    if (link.endTime) {
      // passed end time
      if (link.endTime <= currentTime) {
        return {use: false};
      }
      // otherwise link is still ok, but we need to set timeoutDate
      return {use: true, timeoutDate: link.endTime};
    }
    // if we are here, the link is ok and no timeoutDate needed
    return {use: true};
  },

  /**
   * Report some action on a newtab page (view, click)
   * @param sites Array of sites shown on newtab page
   * @param action String of the behavior to report
   * @param triggeringSiteIndex optional Int index of the site triggering action
   * @return download promise
   */
  reportSitesAction: function DirectoryLinksProvider_reportSitesAction(sites, action, triggeringSiteIndex) {
    // Check if the suggested tile was shown
    if (action == "view") {
      sites.slice(0, triggeringSiteIndex + 1).forEach(site => {
        let {targetedSite, url} = site.link;
        if (targetedSite) {
          this._addFrequencyCapView(url);
        }
      });
    }
    // Use up all views if the user clicked on a frequency capped tile
    else if (action == "click") {
      let {targetedSite, url} = sites[triggeringSiteIndex].link;
      if (targetedSite) {
        this._setFrequencyCapClick(url);
      }
    }

    let newtabEnhanced = false;
    let pingEndPoint = "";
    try {
      newtabEnhanced = Services.prefs.getBoolPref(PREF_NEWTAB_ENHANCED);
      pingEndPoint = Services.prefs.getCharPref(PREF_DIRECTORY_PING);
    }
    catch (ex) {}

    // Only send pings when enhancing tiles with an endpoint and valid action
    let invalidAction = PING_ACTIONS.indexOf(action) == -1;
    if (!newtabEnhanced || pingEndPoint == "" || invalidAction) {
      return Promise.resolve();
    }

    let actionIndex;
    let data = {
      locale: this.locale,
      tiles: sites.reduce((tiles, site, pos) => {
        // Only add data for non-empty tiles
        if (site) {
          // Remember which tiles data triggered the action
          let {link} = site;
          let tilesIndex = tiles.length;
          if (triggeringSiteIndex == pos) {
            actionIndex = tilesIndex;
          }

          // Make the payload in a way so keys can be excluded when stringified
          let id = link.directoryId;
          tiles.push({
            id: id || site.enhancedId,
            pin: site.isPinned() ? 1 : undefined,
            pos: pos != tilesIndex ? pos : undefined,
            score: Math.round(link.frecency / PING_SCORE_DIVISOR) || undefined,
            url: site.enhancedId && "",
          });
        }
        return tiles;
      }, []),
    };

    // Provide a direct index to the tile triggering the action
    if (actionIndex !== undefined) {
      data[action] = actionIndex;
    }

    // Package the data to be sent with the ping
    let ping = new XMLHttpRequest();
    ping.open("POST", pingEndPoint + (action == "view" ? "view" : "click"));
    ping.send(JSON.stringify(data));

    return Task.spawn(function* () {
      // since we updated views/clicks we need write _frequencyCaps to disk
      yield this._writeFrequencyCapFile();
      // Use this as an opportunity to potentially fetch new links
      yield this._fetchAndCacheLinksIfNecessary();
    }.bind(this));
  },

  /**
   * Get the enhanced link object for a link (whether history or directory)
   */
  getEnhancedLink: function DirectoryLinksProvider_getEnhancedLink(link) {
    // Use the provided link if it's already enhanced
    return link.enhancedImageURI && link ? link :
           this._enhancedLinks.get(NewTabUtils.extractSite(link.url));
  },

  /**
   * Get the display name of an allowed frecent sites. Returns undefined for a
   * unallowed frecent sites.
   */
  getFrecentSitesName(sites) {
    return ALLOWED_FRECENT_SITES.get(sites.join(","));
  },

  /**
   * Check if a url's scheme is in a Set of allowed schemes
   */
  isURLAllowed: function DirectoryLinksProvider_isURLAllowed(url, allowed) {
    // Assume no url is an allowed url
    if (!url) {
      return true;
    }

    let scheme = "";
    try {
      // A malformed url will not be allowed
      scheme = Services.io.newURI(url, null, null).scheme;
    }
    catch(ex) {}
    return allowed.has(scheme);
  },

  /**
   * Gets the current set of directory links.
   * @param aCallback The function that the array of links is passed to.
   */
  getLinks: function DirectoryLinksProvider_getLinks(aCallback) {
    this._readDirectoryLinksFile().then(rawLinks => {
      // Reset the cache of suggested tiles and enhanced images for this new set of links
      this._enhancedLinks.clear();
      this._suggestedLinks.clear();
      this._clearCampaignTimeout();

      let validityFilter = function(link) {
        // Make sure the link url is allowed and images too if they exist
        return this.isURLAllowed(link.url, ALLOWED_LINK_SCHEMES) &&
               this.isURLAllowed(link.imageURI, ALLOWED_IMAGE_SCHEMES) &&
               this.isURLAllowed(link.enhancedImageURI, ALLOWED_IMAGE_SCHEMES);
      }.bind(this);

      rawLinks.suggested.filter(validityFilter).forEach((link, position) => {
        // Only allow suggested links with approved frecent sites
        let name = this.getFrecentSitesName(link.frecent_sites);
        if (name == undefined) {
          return;
        }

        link.targetedName = name;
        link.lastVisitDate = rawLinks.suggested.length - position;

        // We cache suggested tiles here but do not push any of them in the links list yet.
        // The decision for which suggested tile to include will be made separately.
        this._cacheSuggestedLinks(link);
        this._updateFrequencyCapSettings(link);
      });

      rawLinks.enhanced.filter(validityFilter).forEach((link, position) => {
        link.lastVisitDate = rawLinks.enhanced.length - position;

        // Stash the enhanced image for the site
        if (link.enhancedImageURI) {
          this._enhancedLinks.set(NewTabUtils.extractSite(link.url), link);
        }
      });

      let links = rawLinks.directory.filter(validityFilter).map((link, position) => {
        link.lastVisitDate = rawLinks.directory.length - position;
        link.frecency = DIRECTORY_FRECENCY;
        return link;
      });

      // Allow for one link suggestion on top of the default directory links
      this.maxNumLinks = links.length + 1;

      // prune frequency caps of outdated urls
      this._pruneFrequencyCapUrls();
      // write frequency caps object to disk asynchronously
      this._writeFrequencyCapFile();

      return links;
    }).catch(ex => {
      Cu.reportError(ex);
      return [];
    }).then(links => {
      aCallback(links);
      this._populatePlacesLinks();
    });
  },

  init: function DirectoryLinksProvider_init() {
    this._setDefaultEnhanced();
    this._addPrefsObserver();
    // setup directory file path and last download timestamp
    this._directoryFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, DIRECTORY_LINKS_FILE);
    this._lastDownloadMS = 0;

    // setup frequency cap file path
    this._frequencyCapFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, FREQUENCY_CAP_FILE);

    NewTabUtils.placesProvider.addObserver(this);
    NewTabUtils.links.addObserver(this);

    return Task.spawn(function() {
      // get the last modified time of the links file if it exists
      let doesFileExists = yield OS.File.exists(this._directoryFilePath);
      if (doesFileExists) {
        let fileInfo = yield OS.File.stat(this._directoryFilePath);
        this._lastDownloadMS = Date.parse(fileInfo.lastModificationDate);
      }
      // read frequency cap file
      yield this._readFrequencyCapFile();
      // fetch directory on startup without force
      yield this._fetchAndCacheLinksIfNecessary();
    }.bind(this));
  },

  _handleManyLinksChanged: function() {
    this._topSitesWithSuggestedLinks.clear();
    this._suggestedLinks.forEach((suggestedLinks, site) => {
      if (NewTabUtils.isTopPlacesSite(site)) {
        this._topSitesWithSuggestedLinks.add(site);
      }
    });
    this._updateSuggestedTile();
  },

  /**
   * Updates _topSitesWithSuggestedLinks based on the link that was changed.
   *
   * @return true if _topSitesWithSuggestedLinks was modified, false otherwise.
   */
  _handleLinkChanged: function(aLink) {
    let changedLinkSite = NewTabUtils.extractSite(aLink.url);
    let linkStored = this._topSitesWithSuggestedLinks.has(changedLinkSite);

    if (!NewTabUtils.isTopPlacesSite(changedLinkSite) && linkStored) {
      this._topSitesWithSuggestedLinks.delete(changedLinkSite);
      return true;
    }

    if (this._suggestedLinks.has(changedLinkSite) &&
        NewTabUtils.isTopPlacesSite(changedLinkSite) && !linkStored) {
      this._topSitesWithSuggestedLinks.add(changedLinkSite);
      return true;
    }
    return false;
  },

  _populatePlacesLinks: function () {
    NewTabUtils.links.populateProviderCache(NewTabUtils.placesProvider, () => {
      this._handleManyLinksChanged();
    });
  },

  onDeleteURI: function(aProvider, aLink) {
    let {url} = aLink;
    // remove clicked flag for that url and
    // call observer upon disk write completion
    this._removeTileClick(url).then(() => {
      this._callObservers("onDeleteURI", url);
    });
  },

  onClearHistory: function() {
    // remove all clicked flags and call observers upon file write
    this._removeAllTileClicks().then(() => {
      this._callObservers("onClearHistory");
    });
  },

  onLinkChanged: function (aProvider, aLink) {
    // Make sure NewTabUtils.links handles the notification first.
    setTimeout(() => {
      if (this._handleLinkChanged(aLink) || this._shouldUpdateSuggestedTile()) {
        this._updateSuggestedTile();
      }
    }, 0);
  },

  onManyLinksChanged: function () {
    // Make sure NewTabUtils.links handles the notification first.
    setTimeout(() => {
      this._handleManyLinksChanged();
    }, 0);
  },

  _getCurrentTopSiteCount: function() {
    let visibleTopSiteCount = 0;
    for (let link of NewTabUtils.links.getLinks().slice(0, MIN_VISIBLE_HISTORY_TILES)) {
      if (link && (link.type == "history" || link.type == "enhanced")) {
        visibleTopSiteCount++;
      }
    }
    return visibleTopSiteCount;
  },

  _shouldUpdateSuggestedTile: function() {
    let sortedLinks = NewTabUtils.getProviderLinks(this);

    let mostFrecentLink = {};
    if (sortedLinks && sortedLinks.length) {
      mostFrecentLink = sortedLinks[0]
    }

    let currTopSiteCount = this._getCurrentTopSiteCount();
    if ((!mostFrecentLink.targetedSite && currTopSiteCount >= MIN_VISIBLE_HISTORY_TILES) ||
        (mostFrecentLink.targetedSite && currTopSiteCount < MIN_VISIBLE_HISTORY_TILES)) {
      // If mostFrecentLink has a targetedSite then mostFrecentLink is a suggested link.
      // If we have enough history links (8+) to show a suggested tile and we are not
      // already showing one, then we should update (to *attempt* to add a suggested tile).
      // OR if we don't have enough history to show a suggested tile (<8) and we are
      // currently showing one, we should update (to remove it).
      return true;
    }

    return false;
  },

  /**
   * Chooses and returns a suggested tile based on a user's top sites
   * that we have an available suggested tile for.
   *
   * @return the chosen suggested tile, or undefined if there isn't one
   */
  _updateSuggestedTile: function() {
    let sortedLinks = NewTabUtils.getProviderLinks(this);

    if (!sortedLinks) {
      // If NewTabUtils.links.resetCache() is called before getting here,
      // sortedLinks may be undefined.
      return;
    }

    // Delete the current suggested tile, if one exists.
    let initialLength = sortedLinks.length;
    if (initialLength) {
      let mostFrecentLink = sortedLinks[0];
      if (mostFrecentLink.targetedSite) {
        this._callObservers("onLinkChanged", {
          url: mostFrecentLink.url,
          frecency: SUGGESTED_FRECENCY,
          lastVisitDate: mostFrecentLink.lastVisitDate,
          type: mostFrecentLink.type,
        }, 0, true);
      }
    }

    if (this._topSitesWithSuggestedLinks.size == 0 || !this._shouldUpdateSuggestedTile()) {
      // There are no potential suggested links we can show or not
      // enough history for a suggested tile.
      return;
    }

    // Create a flat list of all possible links we can show as suggested.
    // Note that many top sites may map to the same suggested links, but we only
    // want to count each suggested link once (based on url), thus possibleLinks is a map
    // from url to suggestedLink. Thus, each link has an equal chance of being chosen at
    // random from flattenedLinks if it appears only once.
    let nextTimeout;
    let possibleLinks = new Map();
    let targetedSites = new Map();
    this._topSitesWithSuggestedLinks.forEach(topSiteWithSuggestedLink => {
      let suggestedLinksMap = this._suggestedLinks.get(topSiteWithSuggestedLink);
      suggestedLinksMap.forEach((suggestedLink, url) => {
        // Skip this link if we've shown it too many times already
        if (!this._testFrequencyCapLimits(url)) {
          return;
        }

        // as we iterate suggestedLinks, check for campaign start/end
        // time limits, and set nextTimeout to the closest timestamp
        let {use, timeoutDate} = this._testLinkForCampaignTimeLimits(suggestedLink);
        // update nextTimeout is necessary
        if (timeoutDate && (!nextTimeout || nextTimeout > timeoutDate)) {
          nextTimeout = timeoutDate;
        }
        // Skip link if it falls outside campaign time limits
        if (!use) {
          return;
        }

        possibleLinks.set(url, suggestedLink);

        // Keep a map of URL to targeted sites. We later use this to show the user
        // what site they visited to trigger this suggestion.
        if (!targetedSites.get(url)) {
          targetedSites.set(url, []);
        }
        targetedSites.get(url).push(topSiteWithSuggestedLink);
      })
    });

    // setup timeout check for starting or ending campaigns
    if (nextTimeout) {
      this._setupCampaignTimeCheck(nextTimeout - Date.now());
    }

    // We might have run out of possible links to show
    let numLinks = possibleLinks.size;
    if (numLinks == 0) {
      return;
    }

    let flattenedLinks = [...possibleLinks.values()];

    // Choose our suggested link at random
    let suggestedIndex = Math.floor(Math.random() * numLinks);
    let chosenSuggestedLink = flattenedLinks[suggestedIndex];

    // Add the suggested link to the front with some extra values
    this._callObservers("onLinkChanged", Object.assign({
      frecency: SUGGESTED_FRECENCY,

      // Choose the first site a user has visited as the target. In the future,
      // this should be the site with the highest frecency. However, we currently
      // store frecency by URL not by site.
      targetedSite: targetedSites.get(chosenSuggestedLink.url).length ?
        targetedSites.get(chosenSuggestedLink.url)[0] : null
    }, chosenSuggestedLink));
    return chosenSuggestedLink;
   },

  /**
   * Reads json file, parses its content, and returns resulting object
   * @param json file path
   * @param json object to return in case file read or parse fails
   * @return a promise resolved to a valid object or undefined upon error
   */
  _readJsonFile: Task.async(function* (filePath, nullObject) {
    let jsonObj;
    try {
      let binaryData = yield OS.File.read(filePath);
      let json = gTextDecoder.decode(binaryData);
      jsonObj = JSON.parse(json);
    }
    catch (e) {}
    return jsonObj || nullObject;
  }),

  /**
   * Loads frequency cap object from file and parses its content
   * @return a promise resolved upon load completion
   *         on error or non-exstent file _frequencyCaps is set to empty object
   */
  _readFrequencyCapFile: Task.async(function* () {
    // set _frequencyCaps object to file's content or empty object
    this._frequencyCaps = yield this._readJsonFile(this._frequencyCapFilePath, {});
  }),

  /**
   * Saves frequency cap object to file
   * @return a promise resolved upon file i/o completion
   */
  _writeFrequencyCapFile: function DirectoryLinksProvider_writeFrequencyCapFile() {
    let json = JSON.stringify(this._frequencyCaps || {});
    return OS.File.writeAtomic(this._frequencyCapFilePath, json, {tmpPath: this._frequencyCapFilePath + ".tmp"});
  },

  /**
   * Clears frequency cap object and writes empty json to file
   * @return a promise resolved upon file i/o completion
   */
  _clearFrequencyCap: function DirectoryLinksProvider_clearFrequencyCap() {
    this._frequencyCaps = {};
    return this._writeFrequencyCapFile();
  },

  /**
   * updates frequency cap configuration for a link
   */
  _updateFrequencyCapSettings: function DirectoryLinksProvider_updateFrequencyCapSettings(link) {
    let capsObject = this._frequencyCaps[link.url];
    if (!capsObject) {
      // create an object with empty counts
      capsObject = {
        dailyViews: 0,
        totalViews: 0,
        lastShownDate: 0,
      };
      this._frequencyCaps[link.url] = capsObject;
    }
    // set last updated timestamp
    capsObject.lastUpdated = Date.now();
    // check for link configuration
    if (link.frequency_caps) {
      capsObject.dailyCap = link.frequency_caps.daily || DEFAULT_DAILY_FREQUENCY_CAP;
      capsObject.totalCap = link.frequency_caps.total || DEFAULT_TOTAL_FREQUENCY_CAP;
    }
    else {
      // fallback to defaults
      capsObject.dailyCap = DEFAULT_DAILY_FREQUENCY_CAP;
      capsObject.totalCap = DEFAULT_TOTAL_FREQUENCY_CAP;
    }
  },

  /**
   * Prunes frequency cap objects for outdated links
   * @param timeDetla milliseconds
   *        all cap objects with lastUpdated less than (now() - timeDelta)
   *        will be removed. This is done to remove frequency cap objects
   *        for unused tile urls
   */
  _pruneFrequencyCapUrls: function DirectoryLinksProvider_pruneFrequencyCapUrls(timeDelta = DEFAULT_PRUNE_TIME_DELTA) {
    let timeThreshold = Date.now() - timeDelta;
    Object.keys(this._frequencyCaps).forEach(url => {
      if (this._frequencyCaps[url].lastUpdated <= timeThreshold) {
        delete this._frequencyCaps[url];
      }
    });
  },

  /**
   * Checks if supplied timestamp happened today
   * @param timestamp in milliseconds
   * @return true if the timestamp was made today, false otherwise
   */
  _wasToday: function DirectoryLinksProvider_wasToday(timestamp) {
    let showOn = new Date(timestamp);
    let today = new Date();
    // call timestamps identical if both day and month are same
    return showOn.getDate() == today.getDate() &&
           showOn.getMonth() == today.getMonth() &&
           showOn.getYear() == today.getYear();
  },

  /**
   * adds some number of views for a url
   * @param url String url of the suggested link
   */
  _addFrequencyCapView: function DirectoryLinksProvider_addFrequencyCapView(url) {
    let capObject = this._frequencyCaps[url];
    // sanity check
    if (!capObject) {
      return;
    }

    // if the day is new: reset the daily counter and lastShownDate
    if (!this._wasToday(capObject.lastShownDate)) {
      capObject.dailyViews = 0;
      // update lastShownDate
      capObject.lastShownDate = Date.now();
    }

    // bump both dialy and total counters
    capObject.totalViews++;
    capObject.dailyViews++;

    // if any of the caps is reached - update suggested tiles
    if (capObject.totalViews >= capObject.totalCap ||
        capObject.dailyViews >= capObject.dailyCap) {
      this._updateSuggestedTile();
    }
  },

  /**
   * Sets clicked flag for link url
   * @param url String url of the suggested link
   */
  _setFrequencyCapClick: function DirectoryLinksProvider_reportFrequencyCapClick(url) {
    let capObject = this._frequencyCaps[url];
    // sanity check
    if (!capObject) {
      return;
    }
    capObject.clicked = true;
    // and update suggested tiles, since current tile became invalid
    this._updateSuggestedTile();
  },

  /**
   * Tests frequency cap limits for link url
   * @param url String url of the suggested link
   * @return true if link is viewable, false otherwise
   */
  _testFrequencyCapLimits: function DirectoryLinksProvider_testFrequencyCapLimits(url) {
    let capObject = this._frequencyCaps[url];
    // sanity check: if url is missing - do not show this tile
    if (!capObject) {
      return false;
    }

    // check for clicked set or total views reached
    if (capObject.clicked || capObject.totalViews >= capObject.totalCap) {
      return false;
    }

    // otherwise check if link is over daily views limit
    if (this._wasToday(capObject.lastShownDate) &&
        capObject.dailyViews >= capObject.dailyCap) {
      return false;
    }

    // we passed all cap tests: return true
    return true;
  },

  /**
   * Removes clicked flag from frequency cap entry for tile landing url
   * @param url String url of the suggested link
   * @return promise resolved upon disk write completion
   */
  _removeTileClick: function DirectoryLinksProvider_removeTileClick(url = "") {
    // remove trailing slash, to accomodate Places sending site urls ending with '/'
    let noTrailingSlashUrl = url.replace(/\/$/,"");
    let capObject = this._frequencyCaps[url] || this._frequencyCaps[noTrailingSlashUrl];
    // return resolved promise if capObject is not found
    if (!capObject) {
      return Promise.resolve();
    }
    // otherwise remove clicked flag
    delete capObject.clicked;
    return this._writeFrequencyCapFile();
  },

  /**
   * Removes all clicked flags from frequency cap object
   * @return promise resolved upon disk write completion
   */
  _removeAllTileClicks: function DirectoryLinksProvider_removeAllTileClicks() {
    Object.keys(this._frequencyCaps).forEach(url => {
      delete this._frequencyCaps[url].clicked;
    });
    return this._writeFrequencyCapFile();
  },

  /**
   * Return the object to its pre-init state
   */
  reset: function DirectoryLinksProvider_reset() {
    delete this.__linksURL;
    this._removePrefsObserver();
    this._removeObservers();
  },

  addObserver: function DirectoryLinksProvider_addObserver(aObserver) {
    this._observers.add(aObserver);
  },

  removeObserver: function DirectoryLinksProvider_removeObserver(aObserver) {
    this._observers.delete(aObserver);
  },

  _callObservers(methodName, ...args) {
    for (let obs of this._observers) {
      if (typeof(obs[methodName]) == "function") {
        try {
          obs[methodName](this, ...args);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  },

  _removeObservers: function() {
    this._observers.clear();
  }
};
