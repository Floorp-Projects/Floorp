/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill field Heuristics RegExp.
 */

/* exported HeuristicsRegExp */

"use strict";

var HeuristicsRegExp = {
  // These regular expressions are from Chromium source codes [1]. Most of them
  // converted to JS format have the same meaning with the original ones except
  // the first line of "address-level1".
  // [1] https://source.chromium.org/chromium/chromium/src/+/master:components/autofill/core/common/autofill_regex_constants.cc
  RULES: {
    // ==== Email ====
    email: new RegExp(
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
        "|(?:이메일|전자.?우편|[Ee]-?mail)(.?주소)?" // ko-KR
          .normalize("NFKC"), // Handle decomposed characters
      "iu"
    ),

    // ==== Telephone ====
    tel: new RegExp(
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
      "iu"
    ),

    // ==== Address Fields ====
    organization: new RegExp(
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
      "iu"
    ),
    "street-address": new RegExp("streetaddress|street-address", "iu"),
    "address-line1": new RegExp(
      "^address$|address[_-]?line(one)?|address1|addr1|street" +
      "|addrline1|address_1" + // Extra rules by Firefox
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
      "iu"
    ),
    "address-line2": new RegExp(
      "address[_-]?line(2|two)|address2|addr2|street|suite|unit" +
      "|addrline2|address_2" + // Extra rules by Firefox
      "|adresszusatz|ergänzende.?angaben" + // de-DE
      "|direccion2|colonia|adicional" + // es
      "|addresssuppl|complementnom|appartement" + // fr-FR
      "|indirizzo2" + // it-IT
      "|住所2" + // ja-JP
      "|complemento|addrcomplement" + // pt-BR, pt-PT
      "|Улица" + // ru
      "|地址2" + // zh-CN
        "|주소.?2", // ko-KR
      "iu"
    ),
    "address-line3": new RegExp(
      "address[_-]?line(3|three)|address3|addr3|street|suite|unit" +
      "|addrline3|address_3" + // Extra rules by Firefox
      "|adresszusatz|ergänzende.?angaben" + // de-DE
      "|direccion3|colonia|adicional" + // es
      "|addresssuppl|complementnom|appartement" + // fr-FR
      "|indirizzo3" + // it-IT
      "|住所3" + // ja-JP
      "|complemento|addrcomplement" + // pt-BR, pt-PT
      "|Улица" + // ru
      "|地址3" + // zh-CN
        "|주소.?3", // ko-KR
      "iu"
    ),
    "address-level2": new RegExp(
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
        "|^시[^도·・]|시[·・]?군[·・]?구" // ko-KR
          .normalize("NFKC"), // Handle decomposed characters
      "iu"
    ),
    "address-level1": new RegExp(
      "(?<!(united|hist|history).?)state|county|region|province" +
      "|land" + // de-DE - Extra rules by Firefox
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
        "|^시[·・]?도" // ko-KR
          .normalize("NFKC"), // Handle decomposed characters
      "iu"
    ),
    "postal-code": new RegExp(
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
      "iu"
    ),
    country: new RegExp(
      "country|countries" +
      "|país|pais" + // es
      "|(\\b|_)land(\\b|_)(?!.*(mark.*))" + // de-DE landmark is a type in india.
      "|(?<!(入|出))国" + // ja-JP
      "|国家" + // zh-CN
      "|국가|나라" + // ko-KR
      "|(\\b|_)(ülke|ulce|ulke)(\\b|_)" + // tr
        "|کشور", // fa
      "iu"
    ),

    // ==== Name Fields ====
    name: new RegExp(
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
      "iu"
    ),
    "given-name": new RegExp(
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
      "iu"
    ),
    "additional-name": new RegExp(
      "middle.*name|mname|middle$" +
      "|apellido.?materno|lastlastname" + // Extra rules by Firefox
        "middle.*initial|m\\.i\\.|mi$|\\bmi\\b", // es
      "iu"
    ),
    "family-name": new RegExp(
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
      "iu"
    ),

    // ==== Credit Card Fields ====
    "cc-name": new RegExp(
      "card.?(?:holder|owner)|name.*(\\b)?on(\\b)?.*card" +
      "|(?:card|cc).?name|cc.?full.?name" +
      "|karteninhaber" + // de-DE
      "|nombre.*tarjeta" + // es
      "|nom.*carte" + // fr-FR
      "|nome.*cart" + // it-IT
      "|名前" + // ja-JP
      "|Имя.*карты" + // ru
      "|信用卡开户名|开户名|持卡人姓名" + // zh-CN
        "|持卡人姓名", // zh-TW
      "iu"
    ),
    "cc-number": new RegExp(
      "(add)?(?:card|cc|acct).?(?:number|#|no|num|field)" +
      "|(cc|kk)nr" + // Extra rules by Firefox for de-DE
      "|(?<!telefon|haus|person|fødsels)nummer" + // de-DE, sv-SE, no
      "|カード番号" + // ja-JP
      "|Номер.*карты" + // ru
      "|信用卡号|信用卡号码" + // zh-CN
      "|信用卡卡號" + // zh-TW
      "|카드" + // ko-KR
        // es/pt/fr
        "|(numero|número|numéro)(?!.*(document|fono|phone|réservation))",
      "iu"
    ),
    "cc-exp-month": new RegExp(
      "expir|exp.*mo|exp.*date|ccmonth|cardmonth|addmonth" +
      "|(cc|kk)month" + // Extra rules by Firefox for de-DE
      "|gueltig|gültig|monat" + // de-DE
      "|fecha" + // es
      "|date.*exp" + // fr-FR
      "|scadenza" + // it-IT
      "|有効期限" + // ja-JP
      "|validade" + // pt-BR, pt-PT
      "|Срок действия карты" + // ru
        "|月", // zh-CN
      "iu"
    ),
    "cc-exp-year": new RegExp(
      "exp|^/|(add)?year" +
      "|(cc|kk)year" + // Extra rules by Firefox for de-DE
      "|ablaufdatum|gueltig|gültig|jahr" + // de-DE
      "|fecha" + // es
      "|scadenza" + // it-IT
      "|有効期限" + // ja-JP
      "|validade" + // pt-BR, pt-PT
      "|Срок действия карты" + // ru
        "|年|有效期", // zh-CN
      "iu"
    ),
    "cc-exp": new RegExp(
      "expir|exp.*date|^expfield$" +
      "|gueltig|gültig" + // de-DE
      "|fecha" + // es
      "|date.*exp" + // fr-FR
      "|scadenza" + // it-IT
      "|有効期限" + // ja-JP
      "|validade" + // pt-BR, pt-PT
        "|Срок действия карты", // ru
      "iu"
    ),
  },
};
