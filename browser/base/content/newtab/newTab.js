/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/BackgroundPageThumbs.jsm");
Cu.import("resource://gre/modules/DirectoryLinksProvider.jsm");
Cu.import("resource://gre/modules/NewTabUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Rect",
  "resource://gre/modules/Geometry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
  "resource://gre/modules/UpdateChannel.jsm");

let {
  links: gLinks,
  allPages: gAllPages,
  linkChecker: gLinkChecker,
  pinnedLinks: gPinnedLinks,
  blockedLinks: gBlockedLinks,
  gridPrefs: gGridPrefs
} = NewTabUtils;

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.
    createBundle("chrome://browser/locale/newTab.properties");
});

const enhancedStringNames = [
  "customize.title",
  "customize.enhanced",
  "customize.classic",
  "customize.blank",
  "customize.what",
  "intro.header",
  "intro.paragraph1",
  "intro.paragraph2",
  "intro.paragraph3",
  "sponsored.button",
  "sponsored.explain",
  "enhanced.explain",
  "learn.link",
  "privacy.link",
];

#ifdef 0
let strings = {};
const matchers = {
  dtd: line => line.match(/^<!ENTITY\s*([^\s]+)\s*"(.+)"\s*>\s*$/),
  properties: line => line.match(/^\s*([^=\s]+)\s*=\s*(.+)\s*$/),
};
const locales = ["en-US", "de", "es-ES", "fr", "ja", "pl", "pt-BR", "ru"];
const files = {
  ja: {
    dtd: "https://bugzilla.mozilla.org/attachment.cgi?id=8481860",
    properties: "https://bugzilla.mozilla.org/attachment.cgi?id=8481859",
  },
  "pt-BR": {
    dtd: "https://bugzilla.mozilla.org/attachment.cgi?id=8481678",
    properties: "https://bugzilla.mozilla.org/attachment.cgi?id=8481677",
  },
};
locales.forEach(locale => {
  strings[locale] = {};
  for (let [type, matcher] of Iterator(matchers)) {
    let x = new XMLHttpRequest();
    let repository = locale == "en-US" ? "mozilla-central/raw-file/tip/browser/locales/en-US/" : "releases/l10n/mozilla-aurora/" + locale + "/raw-file/tip/browser";
    x.open("GET", files[locale] && files[locale][type] || "http://hg.mozilla.org/" + repository + "/chrome/browser/newTab." + type, false);
    x.overrideMimeType("text/plain");
    x.send();
    x.responseText.split("\n").map(matcher).forEach(matches => {
      if (matches) {
        strings[locale][matches[1]] = matches[2];
      }
    });
  }
});
let enhancedStrings = enhancedStringNames.map(name => locales.map(locale => strings[locale]["newtab." + name]));
console.log("const enhancedStrings = " + JSON.stringify(enhancedStrings, undefined, 2) + ";");
#endif

const enhancedStrings = [
  [
    "Customize your New Tab page",
    "Neuer-Tab-Seite anpassen",
    "Personalizar su página Nueva pestaña",
    "Personnaliser la page « Nouvel onglet »",
    "新しいタブページをカスタマイズ",
    "Dostosuj stronę nowej karty",
    "Personalize sua página de nova aba",
    "Настройка страницы «Новая вкладка»"
  ],
  [
    "Enhanced",
    "Erweitert",
    "Mejorada",
    "Améliorée",
    "強調",
    "Rozszerzona",
    "Melhorada",
    "Улучшенная"
  ],
  [
    "Classic",
    "Klassisch",
    "Clásica",
    "Classique",
    "クラシック",
    "Klasyczna",
    "Clássica",
    "Классическая"
  ],
  [
    "Blank",
    "Leer",
    "En blanco",
    "Vide",
    "空白",
    "Pusta",
    "Em branco",
    "Пустая"
  ],
  [
    "What is this page?",
    "Über diese Seite",
    "¿Qué es esta página?",
    "Qu'est-ce que cette page ?",
    "このページについて",
    "Co to za strona?",
    "O que é essa página?",
    "Что это за страница?"
  ],
  [
    "What is this page?",
    "Über diese Seite",
    "¿Qué es esta página?",
    "Qu'est-ce que cette page ?",
    "このページについて",
    "Co to za strona?",
    "O que é essa página?",
    "Что это за страница?"
  ],
  [
    "When you open a new tab, you’ll see tiles from the sites you frequently visit, along with tiles that we think might be of interest to you. Some of these tiles may be sponsored by Mozilla partners. We’ll always indicate to you which tiles are sponsored. %1$S",
    "Wenn Sie einen neuen Tab öffnen, sehen Sie Kacheln für die Webseiten, welche Sie häufig besuchen oder die für Sie interessant sein könnten. Einige dieser Kacheln können von Mozilla-Partnerunternehmen stammen. In diesem Fall sehen Sie bei der Kachel einen Hinweis. %1$S",
    "Al abrir una pestaña nueva, verá miniaturas de los sitios que visita con frecuencia, junto con miniaturas que creemos que pueden interesarle. Algunas de éstas pueden estar patrocinadas por socios de Mozilla. Siempre le indicaremos qué miniaturas son patrocinadas. %1$S",
    "Lorsque vous ouvrez un nouvel onglet, vous apercevrez des vignettes en provenance des sites que vous visitez le plus souvent, mais aussi des vignettes que nous pensons pouvoir vous intéresser. Certaines peuvent être parrainées par des partenaires de Mozilla. Nous vous indiquerons toujours lesquelles sont parrainées. %1$S",
    "新しいタブを開くと、よく訪れるサイトのタイルが、あなたへのおすすめサイトのタイルとともに表示されます。その中には Mozilla のパートナーがスポンサーとなっているものも含まれる場合があります。それらはスポンサーによるものであることが常に明記されています。 %1$S",
    "W nowych kartach wyświetlane są miniaturki często odwiedzanych stron oraz miniaturki, które naszym zdaniem mogą być dla Ciebie interesujące. Niektóre z nich mogą być sponsorowane przez partnerów Mozilli i zawsze będą stosownie oznaczane. %1$S",
    "Ao abrir uma nova aba, será possível ver blocos dos sites que você visita com frequência, além de blocos que consideramos interessantes para você. Alguns desses blocos podem ser patrocinados por parceiros da Mozilla. Sempre indicaremos quais blocos são patrocinados. %1$S",
    "Открыв новую вкладку, вы увидите плитки сайтов, которые вы часто посещаете, а также плитки, которые, по нашему мнению, могут быть вам интересны. Некоторые из этих плиток могут спонсироваться партнерами Mozilla. Мы всегда указываем, у каких плиток есть спонсоры. %1$S"
  ],
  [
    "In order to provide this service, Mozilla collects and uses certain analytics information relating to your use of the tiles in accordance with our %1$S.",
    "Zur Bereitstellung dieses Dienstes sammelt und verwendet Mozilla bestimmte Analysedaten über Ihre Verwendung der Kacheln in Übereinstimmung mit unserem %1$S.",
    "Con el fin de prestarle este servicio, Mozilla recopila y utiliza cierta información analítica sobre cómo utiliza las miniaturas, conforme a nuestro %1$S.",
    "Afin d'assurer ce service, Mozilla collecte et utilise certaines informations analytiques liées à votre utilisation des vignettes, conformément à notre %1$S.",
    "このサービスの提供にあたって、Mozilla は %1$S に従い、ユーザのタイル使用状況に関する特定の分析情報を収集および使用します。",
    "Aby udostępniać tę funkcję, Mozilla zbiera i wykorzystuje pewne informacje o sposobie wykorzystywania miniaturek, zgodnie z %1$S.",
    "Para prestar esse serviço, a Mozilla coleta e utiliza algumas informações de análises relacionadas ao seu uso dos blocos, de acordo com nosso %1$S.",
    "Для оказания этой услуги Mozilla собирает и использует определенную аналитическую информацию об использовании вами плиток в соответствии с нашим %1$S."
  ],
  [
    "You can turn off the tiles feature by clicking the %1$S button for your preferences.",
    "Sie können die Kachelfunktion ausschalten. Klicken Sie dazu <span style='display:inline-block;'>auf %1$S,</span> um die Einstellungen zu öffnen.",
    "Puede desactivar las miniaturas haciendo clic en el botón %1$S para acceder a las preferencias.",
    "Vous pouvez désactiver à votre convenance la fonctionnalité des vignettes en cliquant sur le bouton %1$S.",
    "このタイル機能は %1$S ボタンをクリックすることで無効化できます。",
    "Wyświetlanie miniaturek można wyłączyć, korzystając z przycisku %1$S.",
    "É possível desativar o recurso de blocos clicando no botão %1$S para acessar suas preferências.",
    "Вы можете отключить эту функцию плиток, щёлкнув по кнопке %1$S."
  ],
  [
    "SPONSORED",
    "ANZEIGE",
    "PATROCINADO",
    "PARRAINÉE",
    "スポンサード",
    "Sponsorowana",
    "PATROCINADO",
    "СПОНСОР"
  ],
  [
    "This tile is being shown to you on behalf of a Mozilla partner. You can remove it at any time by clicking the %1$S button. %2$S",
    "Diese Kachel stammt von einem Mozilla-Partnerunternehmen. Sie können sie jederzeit entfernen, indem Sie auf %1$S klicken. %2$S",
    "Le mostramos esta miniatura en nombre de un socio de Mozilla. Puede quitarla en cualquier momento haciendo clic en el botón %1$S. %2$S",
    "Cette vignette vous est présentée au nom d'un partenaire de Mozilla. Vous pouvez la retirer à tout moment en cliquant sur le bouton %1$S. %2$S",
    "このタイルは Mozilla がパートナーに代わって表示しているものです。%1$S ボタンをクリックすればいつでも削除できます。 %2$S",
    "Ta miniaturka jest wyświetlana w imieniu partnera Mozilli. Miniaturkę można usunąć, używając przycisku %1$S. %2$S",
    "Este bloco está sendo mostrado para você em nome de um parceiro da Mozilla. É possível excluí-lo a qualquer momento clicando no botão %1$S. %2$S",
    "Это плитка отображается для вас от имени партнера Mozilla. Её можно удалить в любое время, щёлкнув по кнопке %1$S. %2$S"
  ],
  [
    "A Mozilla partner has visually enhanced this tile, replacing the screenshot. You can turn off enhanced tiles by clicking the %1$S button for your preferences. %2$S",
    "Ein Mozilla-Partnerunternehmen hat das Erscheinungsbild dieser Kachel verändert. Das Bildschirmfoto wurde ersetzt. Sie können erweiterte Kacheln ausschalten, indem Sie mit %1$S die Einstellungen öffnen. %2$S",
    "Un socio de Mozilla ha mejorado esta miniatura visualmente sustituyendo la captura de pantalla. Puede desactivar las miniaturas mejoradas haciendo clic en el botón %1$S para acceder a las preferencias. %2$S",
    "Un partenaire de Mozilla a amélioré visuellement cette vignette en remplaçant la capture d'écran. Vous pouvez désactiver les vignettes améliorées en cliquant sur le bouton %1$S. %2$S",
    "Mozilla のパートナーは、スクリーンショットを置き換えることで、このタイルを視覚的に強調しています。タイルの強調は %1$S ボタンをクリックすることで無効化できます。 %2$S",
    "Partner Mozilli zmienił wygląd tej miniaturki. Modyfikowanie miniaturek można wyłączyć, korzystając z przycisku %1$S. %2$S",
    "Um parceiro da Mozilla substituiu a imagem da página e melhorou o visual desse bloco. É possível desativar os blocos melhorados clicando no botão %1$S para acessar suas preferências. %2$S",
    "Партнер Mozilla визуально улучшил эту плитку, заменив снимок экрана. Отображение улучшенных плиток можно отключить, щёлкнув по кнопке %1$S. %2$S"
  ],
  [
    "Learn more…",
    "Weitere Informationen…",
    "Saber más…",
    "En savoir plus…",
    "詳細...",
    "Więcej informacji…",
    "Saiba mais…",
    "Подробнее…"
  ],
  [
    "Privacy Notice",
    "Datenschutzhinweis",
    "Aviso sobre privacidad",
    "politique de confidentialité",
    "プライバシー通知",
    "polityką prywatności",
    "Aviso de privacidade",
    "Уведомлением о приватности"
  ]
];

function newTabString(name, args = []) {
  let stringIndex = enhancedStringNames.indexOf(name);
  if (stringIndex != -1) {
    let localeIndex = 0;
    switch (DirectoryLinksProvider.locale.slice(0, 2)) {
      case "en":
        localeIndex = 0;
        break;
      case "de":
        localeIndex = 1;
        break;
      case "es":
        localeIndex = 2;
        break;
      case "fr":
        localeIndex = 3;
        break;
      case "ja":
        localeIndex = 4;
        break;
      case "pl":
        localeIndex = 5;
        break;
      case "pt":
        localeIndex = 6;
        break;
      case "ru":
        localeIndex = 7;
        break;
    }
    return enhancedStrings[stringIndex][localeIndex].replace("%1$S", args[0]).replace("%2$S", args[1]);
  }

  let stringName = "newtab." + name;
  if (args.length == 0) {
    return gStringBundle.GetStringFromName(stringName);
  }
  return gStringBundle.formatStringFromName(stringName, args, args.length);
}

function inPrivateBrowsingMode() {
  return PrivateBrowsingUtils.isWindowPrivate(window);
}

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
const XUL_NAMESPACE = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const TILES_EXPLAIN_LINK = "https://support.mozilla.org/kb/how-do-sponsored-tiles-work";
const TILES_PRIVACY_LINK = "https://www.mozilla.org/privacy/";

#include transformations.js
#include page.js
#include grid.js
#include cells.js
#include sites.js
#include drag.js
#include dragDataHelper.js
#include drop.js
#include dropTargetShim.js
#include dropPreview.js
#include updater.js
#include undo.js
#include search.js
#include customize.js
#include intro.js

// Everything is loaded. Initialize the New Tab Page.
gPage.init();
