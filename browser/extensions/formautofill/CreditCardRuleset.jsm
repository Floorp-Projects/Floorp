/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Fathom ML model for identifying the fields of credit-card forms
 *
 * This is developed out-of-tree at https://github.com/mozilla-services/fathom-
 * form-autofill, where there is also over a GB of training, validation, and
 * testing data. To make changes, do your edits there (whether adding new
 * training pages, adding new rules, or both), retrain and evaluate as
 * documented at https://mozilla.github.io/fathom/training.html, paste the
 * coefficients emitted by the trainer into the ruleset, and finally copy the
 * ruleset's "CODE TO COPY INTO PRODUCTION" section to this file's "CODE FROM
 * TRAINING REPOSITORY" section.
 */

"use strict";

/**
 * CODE UNIQUE TO PRODUCTION--NOT IN THE TRAINING REPOSITORY:
 */

const EXPORTED_SYMBOLS = ["creditCardRuleset"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "fathom",
  "resource://gre/modules/third_party/fathom/fathom.jsm"
);
const {
  element: clickedElement,
  out,
  rule,
  ruleset,
  score,
  type,
  utils: { isVisible },
} = fathom;

ChromeUtils.defineModuleGetter(
  this,
  "FormLikeFactory",
  "resource://gre/modules/FormLikeFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://formautofill/FormAutofillUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
  SUPPORTED_NETWORKS: "resource://gre/modules/CreditCard.jsm",
  NETWORK_NAMES: "resource://gre/modules/CreditCard.jsm",
  LabelUtils: "resource://formautofill/FormAutofillUtils.jsm",
});

/**
 * Callthrough abstraction to allow .getAutocompleteInfo() to be mocked out
 * during training
 *
 * @param {Element} element DOM element to get info about
 * @returns {object} Page-author-provided autocomplete metadata
 */
function getAutocompleteInfo(element) {
  return element.getAutocompleteInfo();
}

/**
 * @param {string} selector A CSS selector that prunes away ineligible elements
 * @returns {Lhs} An LHS yielding the element the user has clicked or, if
 *  pruned, none
 */
function queriedOrClickedElements(selector) {
  return clickedElement(selector);
}

/**
 * START OF CODE PASTED FROM TRAINING REPOSITORY
 */

var HeuristicsRegExp = {
  RULES: {
    email: undefined,
    tel: undefined,
    organization: undefined,
    "street-address": undefined,
    "address-line1": undefined,
    "address-line2": undefined,
    "address-line3": undefined,
    "address-level2": undefined,
    "address-level1": undefined,
    "postal-code": undefined,
    country: undefined,
    // Note: We place the `cc-name` field for Credit Card first, because
    // it is more specific than the `name` field below and we want to check
    // for it before we catch the more generic one.
    "cc-name": undefined,
    name: undefined,
    "given-name": undefined,
    "additional-name": undefined,
    "family-name": undefined,
    "cc-number": undefined,
    "cc-exp-month": undefined,
    "cc-exp-year": undefined,
    "cc-exp": undefined,
    "cc-type": undefined,
  },

  RULE_SETS: [
    //=========================================================================
    // Firefox-specific rules
    {
      "address-line1": "addrline1|address_1",
      "address-line2": "addrline2|address_2",
      "address-line3": "addrline3|address_3",
      "address-level1": "land", // de-DE
      "additional-name": "apellido.?materno|lastlastname",
      "cc-name": "account.*holder.*name",
      "cc-number": "(cc|kk)nr", // de-DE
      "cc-exp-month": "(cc|kk)month", // de-DE
      "cc-exp-year": "(cc|kk)year", // de-DE
      "cc-type": "type",
    },

    //=========================================================================
    // These are the rules used by Bitwarden [0], converted into RegExp form.
    // [0] https://github.com/bitwarden/browser/blob/c2b8802201fac5e292d55d5caf3f1f78088d823c/src/services/autofill.service.ts#L436
    {
      email: "(^e-?mail$)|(^email-?address$)",

      tel:
        "(^phone$)" +
        "|(^mobile$)" +
        "|(^mobile-?phone$)" +
        "|(^tel$)" +
        "|(^telephone$)" +
        "|(^phone-?number$)",

      organization:
        "(^company$)" +
        "|(^company-?name$)" +
        "|(^organization$)" +
        "|(^organization-?name$)",

      "street-address":
        "(^address$)" +
        "|(^street-?address$)" +
        "|(^addr$)" +
        "|(^street$)" +
        "|(^mailing-?addr(ess)?$)" + // Modified to not grab lines, below
        "|(^billing-?addr(ess)?$)" + // Modified to not grab lines, below
        "|(^mail-?addr(ess)?$)" + // Modified to not grab lines, below
        "|(^bill-?addr(ess)?$)", // Modified to not grab lines, below

      "address-line1":
        "(^address-?1$)" +
        "|(^address-?line-?1$)" +
        "|(^addr-?1$)" +
        "|(^street-?1$)",

      "address-line2":
        "(^address-?2$)" +
        "|(^address-?line-?2$)" +
        "|(^addr-?2$)" +
        "|(^street-?2$)",

      "address-line3":
        "(^address-?3$)" +
        "|(^address-?line-?3$)" +
        "|(^addr-?3$)" +
        "|(^street-?3$)",

      "address-level2":
        "(^city$)" +
        "|(^town$)" +
        "|(^address-?level-?2$)" +
        "|(^address-?city$)" +
        "|(^address-?town$)",

      "address-level1":
        "(^state$)" +
        "|(^province$)" +
        "|(^provence$)" +
        "|(^address-?level-?1$)" +
        "|(^address-?state$)" +
        "|(^address-?province$)",

      "postal-code":
        "(^postal$)" +
        "|(^zip$)" +
        "|(^zip2$)" +
        "|(^zip-?code$)" +
        "|(^postal-?code$)" +
        "|(^post-?code$)" +
        "|(^address-?zip$)" +
        "|(^address-?postal$)" +
        "|(^address-?code$)" +
        "|(^address-?postal-?code$)" +
        "|(^address-?zip-?code$)",

      country:
        "(^country$)" +
        "|(^country-?code$)" +
        "|(^country-?name$)" +
        "|(^address-?country$)" +
        "|(^address-?country-?name$)" +
        "|(^address-?country-?code$)",

      name: "(^name$)|full-?name|your-?name",

      "given-name":
        "(^f-?name$)" +
        "|(^first-?name$)" +
        "|(^given-?name$)" +
        "|(^first-?n$)",

      "additional-name":
        "(^m-?name$)" +
        "|(^middle-?name$)" +
        "|(^additional-?name$)" +
        "|(^middle-?initial$)" +
        "|(^middle-?n$)" +
        "|(^middle-?i$)",

      "family-name":
        "(^l-?name$)" +
        "|(^last-?name$)" +
        "|(^s-?name$)" +
        "|(^surname$)" +
        "|(^family-?name$)" +
        "|(^family-?n$)" +
        "|(^last-?n$)",

      /* eslint-disable */
      // Let us keep our consistent wrapping.
      "cc-name":
        "cc-?name" +
        "|card-?name" +
        "|cardholder-?name" +
        "|(^nom$)",
      /* eslint-enable */

      "cc-number":
        "cc-?number" +
        "|cc-?num" +
        "|card-?number" +
        "|card-?num" +
        "|cc-?no" +
        "|card-?no" +
        "|numero-?carte" +
        "|num-?carte" +
        "|cb-?num",

      "cc-exp":
        "(^cc-?exp$)" +
        "|(^card-?exp$)" +
        "|(^cc-?expiration$)" +
        "|(^card-?expiration$)" +
        "|(^cc-?ex$)" +
        "|(^card-?ex$)" +
        "|(^card-?expire$)" +
        "|(^card-?expiry$)" +
        "|(^validite$)" +
        "|(^expiration$)" +
        "|(^expiry$)" +
        "|mm-?yy" +
        "|mm-?yyyy" +
        "|yy-?mm" +
        "|yyyy-?mm" +
        "|expiration-?date" +
        "|payment-?card-?expiration" +
        "|(^payment-?cc-?date$)",

      "cc-exp-month":
        "(^exp-?month$)" +
        "|(^cc-?exp-?month$)" +
        "|(^cc-?month$)" +
        "|(^card-?month$)" +
        "|(^cc-?mo$)" +
        "|(^card-?mo$)" +
        "|(^exp-?mo$)" +
        "|(^card-?exp-?mo$)" +
        "|(^cc-?exp-?mo$)" +
        "|(^card-?expiration-?month$)" +
        "|(^expiration-?month$)" +
        "|(^cc-?mm$)" +
        "|(^cc-?m$)" +
        "|(^card-?mm$)" +
        "|(^card-?m$)" +
        "|(^card-?exp-?mm$)" +
        "|(^cc-?exp-?mm$)" +
        "|(^exp-?mm$)" +
        "|(^exp-?m$)" +
        "|(^expire-?month$)" +
        "|(^expire-?mo$)" +
        "|(^expiry-?month$)" +
        "|(^expiry-?mo$)" +
        "|(^card-?expire-?month$)" +
        "|(^card-?expire-?mo$)" +
        "|(^card-?expiry-?month$)" +
        "|(^card-?expiry-?mo$)" +
        "|(^mois-?validite$)" +
        "|(^mois-?expiration$)" +
        "|(^m-?validite$)" +
        "|(^m-?expiration$)" +
        "|(^expiry-?date-?field-?month$)" +
        "|(^expiration-?date-?month$)" +
        "|(^expiration-?date-?mm$)" +
        "|(^exp-?mon$)" +
        "|(^validity-?mo$)" +
        "|(^exp-?date-?mo$)" +
        "|(^cb-?date-?mois$)" +
        "|(^date-?m$)",

      "cc-exp-year":
        "(^exp-?year$)" +
        "|(^cc-?exp-?year$)" +
        "|(^cc-?year$)" +
        "|(^card-?year$)" +
        "|(^cc-?yr$)" +
        "|(^card-?yr$)" +
        "|(^exp-?yr$)" +
        "|(^card-?exp-?yr$)" +
        "|(^cc-?exp-?yr$)" +
        "|(^card-?expiration-?year$)" +
        "|(^expiration-?year$)" +
        "|(^cc-?yy$)" +
        "|(^cc-?y$)" +
        "|(^card-?yy$)" +
        "|(^card-?y$)" +
        "|(^card-?exp-?yy$)" +
        "|(^cc-?exp-?yy$)" +
        "|(^exp-?yy$)" +
        "|(^exp-?y$)" +
        "|(^cc-?yyyy$)" +
        "|(^card-?yyyy$)" +
        "|(^card-?exp-?yyyy$)" +
        "|(^cc-?exp-?yyyy$)" +
        "|(^expire-?year$)" +
        "|(^expire-?yr$)" +
        "|(^expiry-?year$)" +
        "|(^expiry-?yr$)" +
        "|(^card-?expire-?year$)" +
        "|(^card-?expire-?yr$)" +
        "|(^card-?expiry-?year$)" +
        "|(^card-?expiry-?yr$)" +
        "|(^an-?validite$)" +
        "|(^an-?expiration$)" +
        "|(^annee-?validite$)" +
        "|(^annee-?expiration$)" +
        "|(^expiry-?date-?field-?year$)" +
        "|(^expiration-?date-?year$)" +
        "|(^cb-?date-?ann$)" +
        "|(^expiration-?date-?yy$)" +
        "|(^expiration-?date-?yyyy$)" +
        "|(^validity-?year$)" +
        "|(^exp-?date-?year$)" +
        "|(^date-?y$)",

      "cc-type":
        "(^cc-?type$)" +
        "|(^card-?type$)" +
        "|(^card-?brand$)" +
        "|(^cc-?brand$)" +
        "|(^cb-?type$)",
    },

    //=========================================================================
    // These rules are from Chromium source codes [1]. Most of them
    // converted to JS format have the same meaning with the original ones except
    // the first line of "address-level1".
    // [1] https://source.chromium.org/chromium/chromium/src/+/master:components/autofill/core/common/autofill_regex_constants.cc
    {
      // ==== Email ====
      email:
        "e.?mail" +
        "|courriel" + // fr
        "|correo.*electr(o|ó)nico" + // es-ES
        "|メールアドレス" + // ja-JP
        "|Электронной.?Почты" + // ru
        "|邮件|邮箱" + // zh-CN
        "|電郵地址" + // zh-TW
        "|ഇ-മെയില്‍|ഇലക്ട്രോണിക്.?" +
        "മെയിൽ" + // ml
        "|ایمیل|پست.*الکترونیک" + // fa
        "|ईमेल|इलॅक्ट्रॉनिक.?मेल" + // hi
        "|(\\b|_)eposta(\\b|_)" + // tr
        "|(?:이메일|전자.?우편|[Ee]-?mail)(.?주소)?", // ko-KR

      // ==== Telephone ====
      tel:
        "phone|mobile|contact.?number" +
        "|telefonnummer" + // de-DE
        "|telefono|teléfono" + // es
        "|telfixe" + // fr-FR
        "|電話" + // ja-JP
        "|telefone|telemovel" + // pt-BR, pt-PT
        "|телефон" + // ru
        "|मोबाइल" + // hi for mobile
        "|(\\b|_|\\*)telefon(\\b|_|\\*)" + // tr
        "|电话" + // zh-CN
        "|മൊബൈല്‍" + // ml for mobile
        "|(?:전화|핸드폰|휴대폰|휴대전화)(?:.?번호)?", // ko-KR

      // ==== Address Fields ====
      organization:
        "company|business|organization|organisation" +
        "|(?<!con)firma|firmenname" + // de-DE
        "|empresa" + // es
        "|societe|société" + // fr-FR
        "|ragione.?sociale" + // it-IT
        "|会社" + // ja-JP
        "|название.?компании" + // ru
        "|单位|公司" + // zh-CN
        "|شرکت" + // fa
        "|회사|직장", // ko-KR

      "street-address": "streetaddress|street-address",

      "address-line1":
        "^address$|address[_-]?line(one)?|address1|addr1|street" +
        "|(?:shipping|billing)address$" +
        "|strasse|straße|hausnummer|housenumber" + // de-DE
        "|house.?name" + // en-GB
        "|direccion|dirección" + // es
        "|adresse" + // fr-FR
        "|indirizzo" + // it-IT
        "|^住所$|住所1" + // ja-JP
        "|morada|((?<!identificação do )endereço)" + // pt-BR, pt-PT
        "|Адрес" + // ru
        "|地址" + // zh-CN
        "|(\\b|_)adres(?! (başlığı(nız)?|tarifi))(\\b|_)" + // tr
        "|^주소.?$|주소.?1", // ko-KR

      "address-line2":
        "address[_-]?line(2|two)|address2|addr2|street|suite|unit(?!e)" + // Firefox adds `(?!e)` to unit to skip `United State`
        "|adresszusatz|ergänzende.?angaben" + // de-DE
        "|direccion2|colonia|adicional" + // es
        "|addresssuppl|complementnom|appartement" + // fr-FR
        "|indirizzo2" + // it-IT
        "|住所2" + // ja-JP
        "|complemento|addrcomplement" + // pt-BR, pt-PT
        "|Улица" + // ru
        "|地址2" + // zh-CN
        "|주소.?2", // ko-KR

      "address-line3":
        "address[_-]?line(3|three)|address3|addr3|street|suite|unit(?!e)" + // Firefox adds `(?!e)` to unit to skip `United State`
        "|adresszusatz|ergänzende.?angaben" + // de-DE
        "|direccion3|colonia|adicional" + // es
        "|addresssuppl|complementnom|appartement" + // fr-FR
        "|indirizzo3" + // it-IT
        "|住所3" + // ja-JP
        "|complemento|addrcomplement" + // pt-BR, pt-PT
        "|Улица" + // ru
        "|地址3" + // zh-CN
        "|주소.?3", // ko-KR

      "address-level2":
        "city|town" +
        "|\\bort\\b|stadt" + // de-DE
        "|suburb" + // en-AU
        "|ciudad|provincia|localidad|poblacion" + // es
        "|ville|commune" + // fr-FR
        "|localita" + // it-IT
        "|市区町村" + // ja-JP
        "|cidade" + // pt-BR, pt-PT
        "|Город" + // ru
        "|市" + // zh-CN
        "|分區" + // zh-TW
        "|شهر" + // fa
        "|शहर" + // hi for city
        "|ग्राम|गाँव" + // hi for village
        "|നഗരം|ഗ്രാമം" + // ml for town|village
        "|((\\b|_|\\*)([İii̇]l[cç]e(miz|niz)?)(\\b|_|\\*))" + // tr
        "|^시[^도·・]|시[·・]?군[·・]?구", // ko-KR

      "address-level1":
        "(?<!(united|hist|history).?)state|county|region|province" +
        "|county|principality" + // en-UK
        "|都道府県" + // ja-JP
        "|estado|provincia" + // pt-BR, pt-PT
        "|область" + // ru
        "|省" + // zh-CN
        "|地區" + // zh-TW
        "|സംസ്ഥാനം" + // ml
        "|استان" + // fa
        "|राज्य" + // hi
        "|((\\b|_|\\*)(eyalet|[şs]ehir|[İii̇]l(imiz)?|kent)(\\b|_|\\*))" + // tr
        "|^시[·・]?도", // ko-KR

      "postal-code":
        "zip|postal|post.*code|pcode" +
        "|pin.?code" + // en-IN
        "|postleitzahl" + // de-DE
        "|\\bcp\\b" + // es
        "|\\bcdp\\b" + // fr-FR
        "|\\bcap\\b" + // it-IT
        "|郵便番号" + // ja-JP
        "|codigo|codpos|\\bcep\\b" + // pt-BR, pt-PT
        "|Почтовый.?Индекс" + // ru
        "|पिन.?कोड" + // hi
        "|പിന്‍കോഡ്" + // ml
        "|邮政编码|邮编" + // zh-CN
        "|郵遞區號" + // zh-TW
        "|(\\b|_)posta kodu(\\b|_)" + // tr
        "|우편.?번호", // ko-KR

      country:
        "country|countries" +
        "|país|pais" + // es
        "|(\\b|_)land(\\b|_)(?!.*(mark.*))" + // de-DE landmark is a type in india.
        "|(?<!(入|出))国" + // ja-JP
        "|国家" + // zh-CN
        "|국가|나라" + // ko-KR
        "|(\\b|_)(ülke|ulce|ulke)(\\b|_)" + // tr
        "|کشور", // fa

      // ==== Name Fields ====
      "cc-name":
        "card.?(?:holder|owner)|name.*(\\b)?on(\\b)?.*card" +
        "|(?:card|cc).?name|cc.?full.?name" +
        "|(?:card|cc).?owner" +
        "|nombre.*tarjeta" + // es
        "|nom.*carte" + // fr-FR
        "|nome.*cart" + // it-IT
        "|名前" + // ja-JP
        "|Имя.*карты" + // ru
        "|信用卡开户名|开户名|持卡人姓名" + // zh-CN
        "|持卡人姓名", // zh-TW

      name:
        "^name|full.?name|your.?name|customer.?name|bill.?name|ship.?name" +
        "|name.*first.*last|firstandlastname" +
        "|nombre.*y.*apellidos" + // es
        "|^nom(?!bre)" + // fr-FR
        "|お名前|氏名" + // ja-JP
        "|^nome" + // pt-BR, pt-PT
        "|نام.*نام.*خانوادگی" + // fa
        "|姓名" + // zh-CN
        "|(\\b|_|\\*)ad[ı]? soyad[ı]?(\\b|_|\\*)" + // tr
        "|성명", // ko-KR

      "given-name":
        "first.*name|initials|fname|first$|given.*name" +
        "|vorname" + // de-DE
        "|nombre" + // es
        "|forename|prénom|prenom" + // fr-FR
        "|名" + // ja-JP
        "|nome" + // pt-BR, pt-PT
        "|Имя" + // ru
        "|نام" + // fa
        "|이름" + // ko-KR
        "|പേര്" + // ml
        "|(\\b|_|\\*)(isim|ad|ad(i|ı|iniz|ınız)?)(\\b|_|\\*)" + // tr
        "|नाम", // hi

      "additional-name":
        "middle.*name|mname|middle$|middle.*initial|m\\.i\\.|mi$|\\bmi\\b",

      "family-name":
        "last.*name|lname|surname|last$|secondname|family.*name" +
        "|nachname" + // de-DE
        "|apellidos?" + // es
        "|famille|^nom(?!bre)" + // fr-FR
        "|cognome" + // it-IT
        "|姓" + // ja-JP
        "|apelidos|surename|sobrenome" + // pt-BR, pt-PT
        "|Фамилия" + // ru
        "|نام.*خانوادگی" + // fa
        "|उपनाम" + // hi
        "|മറുപേര്" + // ml
        "|(\\b|_|\\*)(soyisim|soyad(i|ı|iniz|ınız)?)(\\b|_|\\*)" + // tr
        "|\\b성(?:[^명]|\\b)", // ko-KR

      // ==== Credit Card Fields ====
      // Note: `cc-name` expression has been moved up, above `name`, in
      // order to handle specialization through ordering.
      "cc-number":
        "(add)?(?:card|cc|acct).?(?:number|#|no|num)" +
        "|(?<!telefon|haus|person|fødsels|zimmer)nummer" + // de-DE, sv-SE, no
        "|カード番号" + // ja-JP
        "|Номер.*карты" + // ru
        "|信用卡号|信用卡号码" + // zh-CN
        "|信用卡卡號" + // zh-TW
        "|카드" + // ko-KR
        // es/pt/fr
        "|(numero|número|numéro)(?!.*(document|fono|phone|réservation))",

      "cc-exp-month":
        "exp.*mo|ccmonth|cardmonth|addmonth" +
        "|monat" + // de-DE
        "|月", // zh-CN

      "cc-exp-year":
        "(add)?year" +
        "|jahr" + // de-DE
        "|年|有效期", // zh-CN

      "cc-exp":
        "expir|exp.*date|^expfield$" +
        "|ablaufdatum|gueltig|gültig" + // de-DE
        "|fecha" + // es
        "|date.*exp" + // fr-FR
        "|scadenza" + // it-IT
        "|有効期限" + // ja-JP
        "|validade" + // pt-BR, pt-PT
        "|Срок действия карты", // ru
    },
  ],

  _getRule(name) {
    let rules = [];
    this.RULE_SETS.forEach(set => {
      if (set[name]) {
        rules.push(`(${set[name]})`.normalize("NFKC"));
      }
    });

    const value = new RegExp(rules.join("|"), "iu");
    Object.defineProperty(this.RULES, name, { get: undefined });
    Object.defineProperty(this.RULES, name, { value });
    return value;
  },

  init() {
    Object.keys(this.RULES).forEach(field =>
      Object.defineProperty(this.RULES, field, {
        get() {
          return HeuristicsRegExp._getRule(field);
        },
      })
    );
  },
};

HeuristicsRegExp.init();

const MMRegExp = /^mm$|\(mm\)/i;
const YYorYYYYRegExp = /^(yy|yyyy)$|\(yy\)|\(yyyy\)/i;
const monthRegExp = /month/i;
const yearRegExp = /year/i;
const MMYYRegExp = /mm\s*(\/|\\)\s*yy/i;
const VisaCheckoutRegExp = /visa(-|\s)checkout/i;
const CREDIT_CARD_NETWORK_REGEXP = new RegExp(
  SUPPORTED_NETWORKS.concat(Object.keys(NETWORK_NAMES)).join("|"),
  "gui"
);
const TwoDigitYearRegExp = /(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yy(?:[^y]|$)/i;
const FourDigitYearRegExp = /(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yyyy(?:[^y]|$)/i;
const dwfrmRegExp = /^dwfrm/i;
const bmlRegExp = /bml/i;
const templatedValue = /^\{\{.*\}\}$/;
const firstRegExp = /first/i;
const lastRegExp = /last/i;
const giftRegExp = /gift/i;
const subscriptionRegExp = /subscription/i;

function autocompleteStringMatches(element, ccString) {
  const info = getAutocompleteInfo(element);
  return info.fieldName === ccString;
}

function getFillableFormElements(element) {
  const formLike = FormLikeFactory.createFromField(element);
  return Array.from(formLike.elements).filter(el =>
    FormAutofillUtils.isFieldEligibleForAutofill(el)
  );
}

function nextFillableFormField(element) {
  const fillableFormElements = getFillableFormElements(element);
  const elementIndex = fillableFormElements.indexOf(element);
  return fillableFormElements[elementIndex + 1];
}

function previousFillableFormField(element) {
  const fillableFormElements = getFillableFormElements(element);
  const elementIndex = fillableFormElements.indexOf(element);
  return fillableFormElements[elementIndex - 1];
}

function nextFieldPredicateIsTrue(element, predicate) {
  const nextField = nextFillableFormField(element);
  return !!nextField && predicate(nextField);
}

function previousFieldPredicateIsTrue(element, predicate) {
  const previousField = previousFillableFormField(element);
  return !!previousField && predicate(previousField);
}

function nextFieldMatchesExpYearAutocomplete(fnode) {
  return nextFieldPredicateIsTrue(fnode.element, nextField =>
    autocompleteStringMatches(nextField, "cc-exp-year")
  );
}

function previousFieldMatchesExpMonthAutocomplete(fnode) {
  return previousFieldPredicateIsTrue(fnode.element, previousField =>
    autocompleteStringMatches(previousField, "cc-exp-month")
  );
}

//////////////////////////////////////////////
// Attribute Regular Expression Rules
function idOrNameMatchRegExp(element, regExp) {
  for (const str of [element.id, element.name]) {
    if (regExp.test(str)) {
      return true;
    }
  }
  return false;
}

function getElementLabels(element) {
  return {
    *[Symbol.iterator]() {
      const labels = LabelUtils.findLabelElements(element);
      for (let label of labels) {
        yield* LabelUtils.extractLabelStrings(label);
      }
    },
  };
}

function labelsMatchRegExp(element, regExp) {
  const elemStrings = getElementLabels(element);
  for (const str of elemStrings) {
    if (regExp.test(str)) {
      return true;
    }
  }
  return false;
}

function ariaLabelMatchesRegExp(element, regExp) {
  const ariaLabel = element.getAttribute("aria-label");
  return !!ariaLabel && regExp.test(ariaLabel);
}

function placeholderMatchesRegExp(element, regExp) {
  const placeholder = element.getAttribute("placeholder");
  return !!placeholder && regExp.test(placeholder);
}

function nextFieldIdOrNameMatchRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    idOrNameMatchRegExp(nextField, regExp)
  );
}

function nextFieldLabelsMatchRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    labelsMatchRegExp(nextField, regExp)
  );
}

function nextFieldPlaceholderMatchesRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    placeholderMatchesRegExp(nextField, regExp)
  );
}

function nextFieldAriaLabelMatchesRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    ariaLabelMatchesRegExp(nextField, regExp)
  );
}

function previousFieldIdOrNameMatchRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    idOrNameMatchRegExp(previousField, regExp)
  );
}

function previousFieldLabelsMatchRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    labelsMatchRegExp(previousField, regExp)
  );
}

function previousFieldPlaceholderMatchesRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    placeholderMatchesRegExp(previousField, regExp)
  );
}

function previousFieldAriaLabelMatchesRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    ariaLabelMatchesRegExp(previousField, regExp)
  );
}
//////////////////////////////////////////////

function isSelectWithCreditCardOptions(fnode) {
  // Check every select for options that match credit card network names in
  // value or label.
  const element = fnode.element;
  if (element.tagName === "SELECT") {
    for (let option of element.querySelectorAll("option")) {
      if (
        CreditCard.getNetworkFromName(option.value) ||
        CreditCard.getNetworkFromName(option.text)
      ) {
        return true;
      }
    }
  }
  return false;
}

/**
 * If any of the regular expressions match multiple times, we assume the tested
 * string belongs to a radio button for payment type instead of card type.
 *
 * @param {Fnode} fnode
 * @returns {boolean}
 */
function isRadioWithCreditCardText(fnode) {
  const element = fnode.element;
  const inputType = element.type;
  if (!!inputType && inputType === "radio") {
    const valueMatches = element.value.match(CREDIT_CARD_NETWORK_REGEXP);
    if (valueMatches) {
      return valueMatches.length === 1;
    }

    // Here we are checking that only one label matches only one entry in the regular expression.
    const labels = getElementLabels(element);
    let labelsMatched = 0;
    for (const label of labels) {
      const labelMatches = label.match(CREDIT_CARD_NETWORK_REGEXP);
      if (labelMatches) {
        if (labelMatches.length > 1) {
          return false;
        }
        labelsMatched++;
      }
    }
    if (labelsMatched > 0) {
      return labelsMatched === 1;
    }

    const textContentMatches = element.textContent.match(
      CREDIT_CARD_NETWORK_REGEXP
    );
    if (textContentMatches) {
      return textContentMatches.length === 1;
    }
  }
  return false;
}

function matchContiguousSubArray(array, subArray) {
  return array.some((elm, i) =>
    subArray.every((sElem, j) => sElem === array[i + j])
  );
}

function isExpirationMonthLikely(element) {
  if (element.tagName !== "SELECT") {
    return false;
  }

  const options = [...element.options];
  const desiredValues = Array(12)
    .fill(1)
    .map((v, i) => v + i);

  // The number of month options shouldn't be less than 12 or larger than 13
  // including the default option.
  if (options.length < 12 || options.length > 13) {
    return false;
  }

  return (
    matchContiguousSubArray(
      options.map(e => +e.value),
      desiredValues
    ) ||
    matchContiguousSubArray(
      options.map(e => +e.label),
      desiredValues
    )
  );
}

function isExpirationYearLikely(element) {
  if (element.tagName !== "SELECT") {
    return false;
  }

  const options = [...element.options];
  // A normal expiration year select should contain at least the last three years
  // in the list.
  const curYear = new Date().getFullYear();
  const desiredValues = Array(3)
    .fill(0)
    .map((v, i) => v + curYear + i);

  return (
    matchContiguousSubArray(
      options.map(e => +e.value),
      desiredValues
    ) ||
    matchContiguousSubArray(
      options.map(e => +e.label),
      desiredValues
    )
  );
}

function nextFieldIsExpirationYearLikely(fnode) {
  return nextFieldPredicateIsTrue(fnode.element, isExpirationYearLikely);
}

function previousFieldIsExpirationMonthLikely(fnode) {
  return previousFieldPredicateIsTrue(fnode.element, isExpirationMonthLikely);
}

function attrsMatchExpWith2Or4DigitYear(fnode, regExpMatchingFunction) {
  const element = fnode.element;
  return (
    regExpMatchingFunction(element, TwoDigitYearRegExp) ||
    regExpMatchingFunction(element, FourDigitYearRegExp)
  );
}

function maxLengthIs(fnode, maxLengthValue) {
  return fnode.element.maxLength === maxLengthValue;
}

function roleIsMenu(fnode) {
  const role = fnode.element.getAttribute("role");
  return !!role && role === "menu";
}

function idOrNameMatchDwfrmAndBml(fnode) {
  return (
    idOrNameMatchRegExp(fnode.element, dwfrmRegExp) &&
    idOrNameMatchRegExp(fnode.element, bmlRegExp)
  );
}

function hasTemplatedValue(fnode) {
  const value = fnode.element.getAttribute("value");
  return !!value && templatedValue.test(value);
}

function inputTypeNotNumbery(fnode) {
  const inputType = fnode.element.type;
  if (inputType) {
    return !["text", "tel", "number"].includes(inputType);
  }
  return false;
}

function isNotVisible(fnode) {
  return !isVisible(fnode);
}

function idOrNameMatchFirstAndLast(fnode) {
  return (
    idOrNameMatchRegExp(fnode.element, firstRegExp) &&
    idOrNameMatchRegExp(fnode.element, lastRegExp)
  );
}

/**
 * Compactly generate a series of rules that all take a single LHS type with no
 * .when() clause and have only a score() call on the right- hand side.
 *
 * @param {Lhs} inType The incoming fnode type that all rules take
 * @param {object} ruleMap A simple object used as a map with rule names
 *   pointing to scoring callbacks
 * @yields {Rule}
 */
function* simpleScoringRules(inType, ruleMap) {
  for (const [name, scoringCallback] of Object.entries(ruleMap)) {
    yield rule(type(inType), score(scoringCallback), { name });
  }
}

function makeRuleset(coeffs, biases) {
  return ruleset(
    [
      /**
       * Factor out the page scan just for a little more speed during training.
       * This selector is good for most fields. cardType is an exception: it
       * cannot be type=month.
       */
      rule(
        queriedOrClickedElements(
          "input:not([type]), input[type=text], input[type=textbox], input[type=email], input[type=tel], input[type=number], input[type=month], select, button"
        ),
        type("typicalCandidates")
      ),

      /**
       * number rules
       */
      rule(type("typicalCandidates"), type("cc-number")),
      ...simpleScoringRules("cc-number", {
        idOrNameMatchNumberRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-number"]
          ),
        labelsMatchNumberRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-number"]),
        placeholderMatchesNumberRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-number"]
          ),
        ariaLabelMatchesNumberRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-number"]
          ),
        idOrNameMatchGift: fnode =>
          idOrNameMatchRegExp(fnode.element, giftRegExp),
        labelsMatchGift: fnode => labelsMatchRegExp(fnode.element, giftRegExp),
        placeholderMatchesGift: fnode =>
          placeholderMatchesRegExp(fnode.element, giftRegExp),
        ariaLabelMatchesGift: fnode =>
          ariaLabelMatchesRegExp(fnode.element, giftRegExp),
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
        isNotVisible,
        inputTypeNotNumbery,
      }),
      rule(type("cc-number"), out("cc-number")),

      /**
       * name rules
       */
      rule(type("typicalCandidates"), type("cc-name")),
      ...simpleScoringRules("cc-name", {
        idOrNameMatchNameRegExp: fnode =>
          idOrNameMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-name"]),
        labelsMatchNameRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-name"]),
        placeholderMatchesNameRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-name"]
          ),
        ariaLabelMatchesNameRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-name"]
          ),
        idOrNameMatchFirst: fnode =>
          idOrNameMatchRegExp(fnode.element, firstRegExp),
        labelsMatchFirst: fnode =>
          labelsMatchRegExp(fnode.element, firstRegExp),
        placeholderMatchesFirst: fnode =>
          placeholderMatchesRegExp(fnode.element, firstRegExp),
        ariaLabelMatchesFirst: fnode =>
          ariaLabelMatchesRegExp(fnode.element, firstRegExp),
        idOrNameMatchLast: fnode =>
          idOrNameMatchRegExp(fnode.element, lastRegExp),
        labelsMatchLast: fnode => labelsMatchRegExp(fnode.element, lastRegExp),
        placeholderMatchesLast: fnode =>
          placeholderMatchesRegExp(fnode.element, lastRegExp),
        ariaLabelMatchesLast: fnode =>
          ariaLabelMatchesRegExp(fnode.element, lastRegExp),
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchFirstAndLast,
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
        isNotVisible,
      }),
      rule(type("cc-name"), out("cc-name")),

      /**
       * cardType rules
       */
      rule(
        queriedOrClickedElements(
          "input:not([type]), input[type=text], input[type=textbox], input[type=email], input[type=tel], input[type=number], input[type=radio], select, button"
        ),
        type("cc-type")
      ),
      ...simpleScoringRules("cc-type", {
        idOrNameMatchTypeRegExp: fnode =>
          idOrNameMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-type"]),
        labelsMatchTypeRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-type"]),
        idOrNameMatchVisaCheckout: fnode =>
          idOrNameMatchRegExp(fnode.element, VisaCheckoutRegExp),
        ariaLabelMatchesVisaCheckout: fnode =>
          ariaLabelMatchesRegExp(fnode.element, VisaCheckoutRegExp),
        isSelectWithCreditCardOptions,
        isRadioWithCreditCardText,
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
      }),
      rule(type("cc-type"), out("cc-type")),

      /**
       * expiration rules
       */
      rule(type("typicalCandidates"), type("cc-exp")),
      ...simpleScoringRules("cc-exp", {
        labelsMatchExpRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-exp"]),
        placeholderMatchesExpRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp"]
          ),
        labelsMatchExpWith2Or4DigitYear: fnode =>
          attrsMatchExpWith2Or4DigitYear(fnode, labelsMatchRegExp),
        placeholderMatchesExpWith2Or4DigitYear: fnode =>
          attrsMatchExpWith2Or4DigitYear(fnode, placeholderMatchesRegExp),
        labelsMatchMMYY: fnode => labelsMatchRegExp(fnode.element, MMYYRegExp),
        placeholderMatchesMMYY: fnode =>
          placeholderMatchesRegExp(fnode.element, MMYYRegExp),
        maxLengthIs7: fnode => maxLengthIs(fnode, 7),
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
        isExpirationMonthLikely: fnode =>
          isExpirationMonthLikely(fnode.element),
        isExpirationYearLikely: fnode => isExpirationYearLikely(fnode.element),
        idOrNameMatchMonth: fnode =>
          idOrNameMatchRegExp(fnode.element, monthRegExp),
        idOrNameMatchYear: fnode =>
          idOrNameMatchRegExp(fnode.element, yearRegExp),
        idOrNameMatchExpMonthRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        idOrNameMatchExpYearRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        idOrNameMatchValidation: fnode =>
          idOrNameMatchRegExp(fnode.element, /validate|validation/i),
      }),
      rule(type("cc-exp"), out("cc-exp")),

      /**
       * expirationMonth rules
       */
      rule(type("typicalCandidates"), type("cc-exp-month")),
      ...simpleScoringRules("cc-exp-month", {
        idOrNameMatchExpMonthRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        labelsMatchExpMonthRegExp: fnode =>
          labelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        placeholderMatchesExpMonthRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        ariaLabelMatchesExpMonthRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        idOrNameMatchMonth: fnode =>
          idOrNameMatchRegExp(fnode.element, monthRegExp),
        labelsMatchMonth: fnode =>
          labelsMatchRegExp(fnode.element, monthRegExp),
        placeholderMatchesMonth: fnode =>
          placeholderMatchesRegExp(fnode.element, monthRegExp),
        ariaLabelMatchesMonth: fnode =>
          ariaLabelMatchesRegExp(fnode.element, monthRegExp),
        nextFieldIdOrNameMatchExpYearRegExp: fnode =>
          nextFieldIdOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldLabelsMatchExpYearRegExp: fnode =>
          nextFieldLabelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldPlaceholderMatchExpYearRegExp: fnode =>
          nextFieldPlaceholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldAriaLabelMatchExpYearRegExp: fnode =>
          nextFieldAriaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldIdOrNameMatchYear: fnode =>
          nextFieldIdOrNameMatchRegExp(fnode.element, yearRegExp),
        nextFieldLabelsMatchYear: fnode =>
          nextFieldLabelsMatchRegExp(fnode.element, yearRegExp),
        nextFieldPlaceholderMatchesYear: fnode =>
          nextFieldPlaceholderMatchesRegExp(fnode.element, yearRegExp),
        nextFieldAriaLabelMatchesYear: fnode =>
          nextFieldAriaLabelMatchesRegExp(fnode.element, yearRegExp),
        nextFieldMatchesExpYearAutocomplete,
        isExpirationMonthLikely: fnode =>
          isExpirationMonthLikely(fnode.element),
        nextFieldIsExpirationYearLikely,
        maxLengthIs2: fnode => maxLengthIs(fnode, 2),
        placeholderMatchesMM: fnode =>
          placeholderMatchesRegExp(fnode.element, MMRegExp),
        roleIsMenu,
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
      }),
      rule(type("cc-exp-month"), out("cc-exp-month")),

      /**
       * expirationYear rules
       */
      rule(type("typicalCandidates"), type("cc-exp-year")),
      ...simpleScoringRules("cc-exp-year", {
        idOrNameMatchExpYearRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        labelsMatchExpYearRegExp: fnode =>
          labelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        placeholderMatchesExpYearRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        ariaLabelMatchesExpYearRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        idOrNameMatchYear: fnode =>
          idOrNameMatchRegExp(fnode.element, yearRegExp),
        labelsMatchYear: fnode => labelsMatchRegExp(fnode.element, yearRegExp),
        placeholderMatchesYear: fnode =>
          placeholderMatchesRegExp(fnode.element, yearRegExp),
        ariaLabelMatchesYear: fnode =>
          ariaLabelMatchesRegExp(fnode.element, yearRegExp),
        previousFieldIdOrNameMatchExpMonthRegExp: fnode =>
          previousFieldIdOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldLabelsMatchExpMonthRegExp: fnode =>
          previousFieldLabelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldPlaceholderMatchExpMonthRegExp: fnode =>
          previousFieldPlaceholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldAriaLabelMatchExpMonthRegExp: fnode =>
          previousFieldAriaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldIdOrNameMatchMonth: fnode =>
          previousFieldIdOrNameMatchRegExp(fnode.element, monthRegExp),
        previousFieldLabelsMatchMonth: fnode =>
          previousFieldLabelsMatchRegExp(fnode.element, monthRegExp),
        previousFieldPlaceholderMatchesMonth: fnode =>
          previousFieldPlaceholderMatchesRegExp(fnode.element, monthRegExp),
        previousFieldAriaLabelMatchesMonth: fnode =>
          previousFieldAriaLabelMatchesRegExp(fnode.element, monthRegExp),
        previousFieldMatchesExpMonthAutocomplete,
        isExpirationYearLikely: fnode => isExpirationYearLikely(fnode.element),
        previousFieldIsExpirationMonthLikely,
        placeholderMatchesYYOrYYYY: fnode =>
          placeholderMatchesRegExp(fnode.element, YYorYYYYRegExp),
        roleIsMenu,
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
      }),
      rule(type("cc-exp-year"), out("cc-exp-year")),
    ],
    coeffs,
    biases
  );
}

const coefficients = {
  "cc-number": [
    ["idOrNameMatchNumberRegExp", 6.203742027282715],
    ["labelsMatchNumberRegExp", 2.7693567276000977],
    ["placeholderMatchesNumberRegExp", 5.2803192138671875],
    ["ariaLabelMatchesNumberRegExp", 4.4139862060546875],
    ["idOrNameMatchGift", -3.9572882652282715],
    ["labelsMatchGift", -4.744204044342041],
    ["placeholderMatchesGift", -1.9791371822357178],
    ["ariaLabelMatchesGift", -3.647447347640991],
    ["idOrNameMatchSubscription", -1.0306156873703003],
    ["idOrNameMatchDwfrmAndBml", -1.239144206047058],
    ["hasTemplatedValue", -5.815555095672607],
    ["isNotVisible", -2.651319980621338],
    ["inputTypeNotNumbery", -2.2579596042633057],
  ],
  "cc-name": [
    ["idOrNameMatchNameRegExp", 7.928648471832275],
    ["labelsMatchNameRegExp", 11.859264373779297],
    ["placeholderMatchesNameRegExp", 8.542611122131348],
    ["ariaLabelMatchesNameRegExp", 10.304289817810059],
    ["idOrNameMatchFirst", -5.997744083404541],
    ["labelsMatchFirst", -14.69614315032959],
    ["placeholderMatchesFirst", -10.472495079040527],
    ["ariaLabelMatchesFirst", -2.1006155014038086],
    ["idOrNameMatchLast", -5.563120365142822],
    ["labelsMatchLast", -14.770337104797363],
    ["placeholderMatchesLast", -8.556757926940918],
    ["ariaLabelMatchesLast", -2.6464316844940186],
    ["idOrNameMatchFirstAndLast", 17.412755966186523],
    ["idOrNameMatchSubscription", -1.5337339639663696],
    ["idOrNameMatchDwfrmAndBml", -1.1979364156723022],
    ["hasTemplatedValue", -1.3346821069717407],
    ["isNotVisible", -20.80698013305664],
  ],
  "cc-type": [
    ["idOrNameMatchTypeRegExp", 2.881605863571167],
    ["labelsMatchTypeRegExp", -2.730766773223877],
    ["idOrNameMatchVisaCheckout", -4.703733444213867],
    ["ariaLabelMatchesVisaCheckout", -4.743120193481445],
    ["isSelectWithCreditCardOptions", 7.484333038330078],
    ["isRadioWithCreditCardText", 9.303544044494629],
    ["idOrNameMatchSubscription", -2.3095338344573975],
    ["idOrNameMatchDwfrmAndBml", -2.4049699306488037],
    ["hasTemplatedValue", -2.239445924758911],
  ],
  "cc-exp": [
    ["labelsMatchExpRegExp", 7.082833290100098],
    ["placeholderMatchesExpRegExp", 3.8282089233398438],
    ["labelsMatchExpWith2Or4DigitYear", 3.2946879863739014],
    ["placeholderMatchesExpWith2Or4DigitYear", 0.9226701259613037],
    ["labelsMatchMMYY", 8.567533493041992],
    ["placeholderMatchesMMYY", 6.9347310066223145],
    ["maxLengthIs7", -1.6171562671661377],
    ["idOrNameMatchSubscription", -1.6738383769989014],
    ["idOrNameMatchDwfrmAndBml", -2.137850046157837],
    ["hasTemplatedValue", -2.517192840576172],
    ["isExpirationMonthLikely", -2.4290213584899902],
    ["isExpirationYearLikely", -1.8661760091781616],
    ["idOrNameMatchMonth", -2.9048914909362793],
    ["idOrNameMatchYear", -2.3091514110565186],
    ["idOrNameMatchExpMonthRegExp", -2.315790891647339],
    ["idOrNameMatchExpYearRegExp", -2.4727766513824463],
    ["idOrNameMatchValidation", -6.610110759735107],
  ],
  "cc-exp-month": [
    ["idOrNameMatchExpMonthRegExp", -4.407843589782715],
    ["labelsMatchExpMonthRegExp", 0.8475375771522522],
    ["placeholderMatchesExpMonthRegExp", -2.3365111351013184],
    ["ariaLabelMatchesExpMonthRegExp", -1.1260497570037842],
    ["idOrNameMatchMonth", 9.68936538696289],
    ["labelsMatchMonth", 0.4420455992221832],
    ["placeholderMatchesMonth", -2.361361503601074],
    ["ariaLabelMatchesMonth", -1.3552377223968506],
    ["nextFieldIdOrNameMatchExpYearRegExp", 1.3826546669006348],
    ["nextFieldLabelsMatchExpYearRegExp", 0.04065725952386856],
    ["nextFieldPlaceholderMatchExpYearRegExp", -2.1881299018859863],
    ["nextFieldAriaLabelMatchExpYearRegExp", 2.4479525089263916],
    ["nextFieldIdOrNameMatchYear", -0.8094764351844788],
    ["nextFieldLabelsMatchYear", -0.2177165001630783],
    ["nextFieldPlaceholderMatchesYear", -2.5288033485412598],
    ["nextFieldAriaLabelMatchesYear", 2.837548017501831],
    ["nextFieldMatchesExpYearAutocomplete", 4.917409420013428],
    ["isExpirationMonthLikely", 13.217621803283691],
    ["nextFieldIsExpirationYearLikely", 4.660866737365723],
    ["maxLengthIs2", -1.1399182081222534],
    ["placeholderMatchesMM", 18.810894012451172],
    ["roleIsMenu", 2.6598381996154785],
    ["idOrNameMatchSubscription", -22.55915069580078],
    ["idOrNameMatchDwfrmAndBml", -22.938968658447266],
    ["hasTemplatedValue", -13.212718963623047],
  ],
  "cc-exp-year": [
    ["idOrNameMatchExpYearRegExp", 2.7729642391204834],
    ["labelsMatchExpYearRegExp", 0.5702193379402161],
    ["placeholderMatchesExpYearRegExp", -0.851945698261261],
    ["ariaLabelMatchesExpYearRegExp", -0.41747674345970154],
    ["idOrNameMatchYear", 2.305432081222534],
    ["labelsMatchYear", 0.8610246777534485],
    ["placeholderMatchesYear", -0.7750431895256042],
    ["ariaLabelMatchesYear", -0.4221137762069702],
    ["previousFieldIdOrNameMatchExpMonthRegExp", -1.500440239906311],
    ["previousFieldLabelsMatchExpMonthRegExp", 0.5963879823684692],
    ["previousFieldPlaceholderMatchExpMonthRegExp", -0.6299140453338623],
    ["previousFieldAriaLabelMatchExpMonthRegExp", 2.0240583419799805],
    ["previousFieldIdOrNameMatchMonth", -1.2178326845169067],
    ["previousFieldLabelsMatchMonth", 0.7169262170791626],
    ["previousFieldPlaceholderMatchesMonth", -0.594235897064209],
    ["previousFieldAriaLabelMatchesMonth", 2.187427043914795],
    ["previousFieldMatchesExpMonthAutocomplete", 1.4384053945541382],
    ["isExpirationYearLikely", 6.711360454559326],
    ["previousFieldIsExpirationMonthLikely", 7.522566795349121],
    ["placeholderMatchesYYOrYYYY", 10.13697624206543],
    ["roleIsMenu", 3.759685754776001],
    ["idOrNameMatchSubscription", -5.86909294128418],
    ["idOrNameMatchDwfrmAndBml", -6.245838642120361],
    ["hasTemplatedValue", -6.6941046714782715],
  ],
};

const biases = [
  ["cc-number", -4.377972602844238],
  ["cc-name", -5.851526260375977],
  ["cc-type", -5.346733570098877],
  ["cc-exp", -5.388509750366211],
  ["cc-exp-month", -7.504126071929932],
  ["cc-exp-year", -6.299252510070801],
];

/**
 * END OF CODE PASTED FROM TRAINING REPOSITORY
 */

/**
 * MORE CODE UNIQUE TO PRODUCTION--NOT IN THE TRAINING REPOSITORY:
 */

this.creditCardRuleset = makeRuleset(
  [
    ...coefficients["cc-number"],
    ...coefficients["cc-name"],
    ...coefficients["cc-type"],
    ...coefficients["cc-exp"],
    ...coefficients["cc-exp-month"],
    ...coefficients["cc-exp-year"],
  ],
  biases
);
