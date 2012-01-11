/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, isApproxVec, vec3, quat4, TiltVisualizer */
/*global Cc, Ci, Cu */
"use strict";

function cloneUpdate(update) {
  return {
    rotation: quat4.create(update.rotation),
    translation: vec3.create(update.translation)
  };
}

function isExpectedUpdate(update1, update2) {
  if (update1.length !== update2.length) {
    return false;
  }
  for (let i = 0, len = update1.length; i < len; i++) {
    if (!isApproxVec(update1[i].rotation, update2[i].rotation) ||
        !isApproxVec(update1[i].translation, update2[i].translation)) {
      return false;
    }
  }
  return true;
}

function test() {
  let arcball1 = new TiltVisualizer.Arcball(123, 456);

  is(arcball1.width, 123,
    "The first arcball width wasn't set correctly.");
  is(arcball1.height, 456,
    "The first arcball height wasn't set correctly.");
  is(arcball1.radius, 123,
    "The first arcball radius wasn't implicitly set correctly.");


  let arcball2 = new TiltVisualizer.Arcball(987, 654);

  is(arcball2.width, 987,
    "The second arcball width wasn't set correctly.");
  is(arcball2.height, 654,
    "The second arcball height wasn't set correctly.");
  is(arcball2.radius, 654,
    "The second arcball radius wasn't implicitly set correctly.");


  let arcball3 = new TiltVisualizer.Arcball(512, 512);

  let sphereVec = vec3.create();
  arcball3.pointToSphere(123, 456, 256, 512, 512, sphereVec);

  ok(isApproxVec(sphereVec, [-0.009765625, 0.390625, 0.9204980731010437]),
    "The pointToSphere() function didn't map the coordinates correctly.");

  let stack1 = [];
  let expect1 = [
    { rotation: [
      -0.054038457572460175, 0.015347825363278389,
      -0.02533721923828125, -0.9980993270874023],
      translation: [0, 0, 0] },
    { rotation: [
      -0.09048379212617874, 0.024709727615118027,
      -0.04307326674461365, -0.9946591854095459],
      translation: [0, 0, 0] },
    { rotation: [
      -0.11537143588066101, 0.03063894622027874,
      -0.05548851564526558, -0.9912980198860168],
      translation: [0, 0, 0] },
    { rotation: [
      -0.13250185549259186, 0.03449848294258118,
      -0.0641791820526123, -0.9885009527206421],
      translation: [0, 0, 0] },
    { rotation: [
      -0.14435507357120514, 0.037062086164951324,
      -0.07026264816522598, -0.9863321781158447],
      translation: [0, 0, 0] },
    { rotation: [
      -0.15258607268333435, 0.03879034146666527,
      -0.07452107220888138, -0.9847128391265869],
      translation: [0, 0, 0] },
    { rotation: [
      -0.1583157479763031, 0.03996811807155609,
      -0.07750196009874344, -0.9835304617881775],
      translation: [0, 0, 0] },
    { rotation: [
      -0.16231097280979156, 0.04077700152993202,
      -0.07958859205245972, -0.982679009437561],
      translation: [0, 0, 0] },
    { rotation: [
      -0.16510005295276642, 0.04133564606308937,
      -0.08104922622442245, -0.9820714592933655],
      translation: [0, 0, 0] },
    { rotation: [
      -0.16704875230789185, 0.04172303527593613,
      -0.08207167685031891, -0.9816405177116394],
      translation: [0, 0, 0] }];

  arcball3.mouseDown(10, 10, 1);
  arcball3.mouseMove(10, 100);
  for (let i1 = 0; i1 < 10; i1++) {
    stack1.push(cloneUpdate(arcball3.update()));
  }

  ok(isExpectedUpdate(stack1, expect1),
    "Mouse down & move events didn't create the expected transform. results.");

  let stack2 = [];
  let expect2 = [
    { rotation: [
      -0.1684110015630722, 0.04199237748980522,
      -0.0827873945236206, -0.9813361167907715],
      translation: [0, 0, 0] },
    { rotation: [
      -0.16936375200748444, 0.04218007251620293,
      -0.08328840136528015, -0.9811217188835144],
      translation: [0, 0, 0] },
    { rotation: [
      -0.17003019154071808, 0.04231100529432297,
      -0.08363909274339676, -0.9809709787368774],
      translation: [0, 0, 0] },
    { rotation: [
      -0.17049652338027954, 0.042402446269989014,
      -0.0838845893740654, -0.9808651208877563],
      translation: [0, 0, 0] },
    { rotation: [
      -0.17082282900810242, 0.042466338723897934,
      -0.08405643701553345, -0.9807908535003662],
      translation: [0, 0, 0] },
    { rotation: [
      -0.17105120420455933, 0.04251104220747948,
      -0.08417671173810959, -0.9807388186454773],
      translation: [0, 0, 0] },
    { rotation: [
      -0.17121103405952454, 0.04254228621721268,
      -0.08426092565059662, -0.9807023406028748],
      translation: [0, 0, 0] },
    { rotation: [
      -0.17132291197776794, 0.042564138770103455,
      -0.08431987464427948, -0.9806767106056213],
      translation: [0, 0, 0] },
    { rotation: [
      -0.1714012324810028, 0.04257945716381073,
      -0.08436112850904465, -0.9806588888168335],
      translation: [0, 0, 0] },
    { rotation: [
      -0.17145603895187378, 0.042590171098709106,
      -0.08439001441001892, -0.9806463718414307],
      translation: [0, 0, 0] }];

  arcball3.mouseUp(100, 100);
  for (let i2 = 0; i2 < 10; i2++) {
    stack2.push(cloneUpdate(arcball3.update()));
  }

  ok(isExpectedUpdate(stack2, expect2),
    "Mouse up events didn't create the expected transformation results.");

  let stack3 = [];
  let expect3 = [
    { rotation: [
      -0.17149439454078674, 0.04259764403104782,
      -0.08441022783517838, -0.9806375503540039],
      translation: [0, 0, -1] },
    { rotation: [
      -0.17152123153209686, 0.04260288551449776,
      -0.08442437648773193, -0.980631411075592],
      translation: [0, 0, -1.899999976158142] },
    { rotation: [
      -0.1715400665998459, 0.04260658100247383,
      -0.08443428575992584, -0.9806271195411682],
      translation: [0, 0, -2.7100000381469727] },
    { rotation: [
      -0.17155319452285767, 0.04260912910103798,
      -0.08444121479988098, -0.9806240797042847],
      translation: [0, 0, -3.439000129699707] },
    { rotation: [
      -0.17156240344047546, 0.042610932141542435,
      -0.08444607257843018, -0.9806219935417175],
      translation: [0, 0, -4.095099925994873] },
    { rotation: [
      -0.1715688556432724, 0.042612191289663315,
      -0.08444946259260178, -0.9806205034255981],
      translation: [0, 0, -4.685589790344238] },
    { rotation: [
      -0.17157337069511414, 0.04261308163404465,
      -0.0844518393278122, -0.980619490146637],
      translation: [0, 0, -5.217031002044678] },
    { rotation: [
      -0.17157652974128723, 0.0426136814057827,
      -0.0844535157084465, -0.9806187748908997],
      translation: [0, 0, -5.6953277587890625] },
    { rotation: [
      -0.17157875001430511, 0.04261413961648941,
      -0.08445467799901962, -0.9806182980537415],
      translation: [0, 0, -6.125794887542725] },
    { rotation: [
      -0.17158031463623047, 0.04261442646384239,
      -0.08445550501346588, -0.980617880821228],
      translation: [0, 0, -6.5132155418396] }];

  arcball3.zoom(10);
  for (let i3 = 0; i3 < 10; i3++) {
    stack3.push(cloneUpdate(arcball3.update()));
  }

  ok(isExpectedUpdate(stack3, expect3),
    "Mouse zoom events didn't create the expected transformation results.");

  let stack4 = [];
  let expect4 = [
    { rotation: [
      -0.17158135771751404, 0.04261462762951851,
      -0.08445606380701065, -0.9806176424026489],
      translation: [0, 0, -6.861894130706787] },
    { rotation: [
      -0.1715821474790573, 0.04261479899287224,
      -0.08445646613836288, -0.9806175231933594],
      translation: [0, 0, -7.1757049560546875] },
    { rotation: [
      -0.1715826541185379, 0.0426148846745491,
      -0.08445674180984497, -0.980617344379425],
      translation: [0, 0, -7.458134651184082] },
    { rotation: [
      -0.17158304154872894, 0.04261497035622597,
      -0.08445693552494049, -0.9806172847747803],
      translation: [0, 0, -7.7123212814331055] },
    { rotation: [
      -0.17158329486846924, 0.042615000158548355,
      -0.08445708453655243, -0.9806172251701355],
      translation: [0, 0, -7.941089153289795] },
    { rotation: [
      -0.17158347368240356, 0.04261505603790283,
      -0.084457166492939, -0.9806172251701355],
      translation: [0, 0, -8.146980285644531] },
    { rotation: [
      -0.1715836226940155, 0.04261508584022522,
      -0.08445724099874496, -0.9806171655654907],
      translation: [0, 0, -8.332282066345215] },
    { rotation: [
      -0.17158368229866028, 0.04261508584022522,
      -0.08445728570222855, -0.980617105960846],
      translation: [0, 0, -8.499053955078125] },
    { rotation: [
      -0.17158377170562744, 0.04261511191725731,
      -0.08445732295513153, -0.980617105960846],
      translation: [0, 0, -8.649148941040039] },
    { rotation: [
      -0.17158380150794983, 0.04261511191725731,
      -0.08445733785629272, -0.980617105960846],
      translation: [0, 0, -8.784234046936035] }];

  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_A);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_D);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_W);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_S);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_LEFT);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_RIGHT);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_UP);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_DOWN);
  for (let i4 = 0; i4 < 10; i4++) {
    stack4.push(cloneUpdate(arcball3.update()));
  }

  ok(isExpectedUpdate(stack4, expect4),
    "Key down events didn't create the expected transformation results.");

  let stack5 = [];
  let expect5 = [
    { rotation: [
      -0.1715838462114334, 0.04261511191725731,
      -0.08445736765861511, -0.980617105960846],
      translation: [0, 0, -8.905810356140137] },
    { rotation: [
      -0.1715838462114334, 0.04261511191725731,
      -0.08445736765861511, -0.980617105960846],
      translation: [0, 0, -9.015229225158691] },
    { rotation: [
      -0.1715838462114334, 0.04261511191725731,
      -0.08445736765861511, -0.980617105960846],
      translation: [0, 0, -9.113706588745117] },
    { rotation: [
      -0.1715838611125946, 0.04261511191725731,
      -0.0844573825597763, -0.9806170463562012],
      translation: [0, 0, -9.202336311340332] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -9.282102584838867] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -9.35389232635498] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -9.418502807617188] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -9.476652145385742] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -9.528986930847168] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -9.576087951660156] }];

  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_A);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_D);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_W);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_S);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_LEFT);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_RIGHT);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_UP);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_DOWN);
  for (let i5 = 0; i5 < 10; i5++) {
    stack5.push(cloneUpdate(arcball3.update()));
  }

  ok(isExpectedUpdate(stack5, expect5),
    "Key up events didn't create the expected transformation results.");

  let stack6 = [];
  let expect6 = [
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -9.618478775024414] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, -6.156630992889404] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 0.4590320587158203] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 9.913128852844238] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 21.921815872192383] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 36.22963333129883] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 52.60667037963867] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 70.84600067138672] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 90.76139831542969] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 112.18525695800781] }];

  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_I);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_ADD);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_EQUALS);
  for (let i6 = 0; i6 < 10; i6++) {
    stack6.push(cloneUpdate(arcball3.update()));
  }
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_I);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_ADD);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_EQUALS);

  ok(isExpectedUpdate(stack6, expect6),
    "Key zoom in events didn't create the expected transformation results.");

  let stack7 = [];
  let expect7 = [
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 134.96673583984375] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 151.97006225585938] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 163.77305603027344] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 170.895751953125] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 173.80618286132812] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 172.92556762695312] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 168.6330108642578] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 161.26971435546875] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 151.1427459716797] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 138.52847290039062] }];

  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_O);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_SUBTRACT);
  for (let i7 = 0; i7 < 10; i7++) {
    stack7.push(cloneUpdate(arcball3.update()));
  }
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_O);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_SUBTRACT);

  ok(isExpectedUpdate(stack7, expect7),
    "Key zoom out events didn't create the expected transformation results.");

  let stack8 = [];
  let expect8 = [
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 123.67562866210938] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 111.30806732177734] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 100.17726135253906] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 90.15953826904297] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 81.14358520507812] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 73.02922821044922] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 65.72630310058594] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 59.15367126464844] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 53.238304138183594] },
    { rotation: [
      -0.17158392071723938, 0.0426151417195797,
      -0.0844573974609375, -0.980617105960846],
      translation: [0, 0, 47.91447448730469] }];

  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_R);
  arcball3.keyDown(Ci.nsIDOMKeyEvent.DOM_VK_0);
  for (let i8 = 0; i8 < 10; i8++) {
    stack8.push(cloneUpdate(arcball3.update()));
  }
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_R);
  arcball3.keyUp(Ci.nsIDOMKeyEvent.DOM_VK_0);

  ok(isExpectedUpdate(stack8, expect8),
    "Key zoom reset events didn't create the expected transformation results.");


  arcball3.resize(123, 456);
  is(arcball3.width, 123,
    "The arcball width wasn't updated correctly.");
  is(arcball3.height, 456,
    "The arcball height wasn't updated correctly.");
  is(arcball3.radius, 123,
    "The arcball radius wasn't implicitly updated correctly.");
}
