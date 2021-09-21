/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CommonNames"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PageDataCollector: "resource:///modules/pagedata/PageDataCollector.jsm",
  shortURL: "resource://activity-stream/lib/ShortURL.jsm",
});

/**
 * A subset of the most visited sites according to Alexa. Used as a fallback if
 * a site doesn't expose site_name metadata. Maps a site's hostname (not
 * including `www.`) to its common name.
 */
const CUSTOM_NAMES = new Map([
  ["adobe.com", "Adobe"],
  ["adp.com", "ADP"],
  ["airbnb.com", "Airbnb"],
  ["alibaba.com", "Alibaba"],
  ["aliexpress.com", "AliExpress"],
  ["aliexpress.ru", "AliExpress.ru"],
  ["allegro.pl", "Allegro"],
  ["amazon.ca", "Amazon.ca"],
  ["amazon.co.jp", "Amazon.co.jp"],
  ["amazon.co.uk", "Amazon.co.uk"],
  ["amazon.com", "Amazon"],
  ["amazon.de", "Amazon.de"],
  ["amazon.es", "Amazon.es"],
  ["amazon.fr", "Amazon.fr"],
  ["amazon.in", "Amazon.in"],
  ["amazon.it", "Amazon.it"],
  ["amazonaws.com", "AWS"],
  ["americanexpress.com", "American Express"],
  ["ameritrade.com", "TD Ameritrade"],
  ["aol.com", "AOL"],
  ["apple.com", "Apple"],
  ["archive.org", "Internet Archive"],
  ["ask.com", "Ask.com"],
  ["att.com", "AT&T"],
  ["aws.amazon.com", "AWS"],
  ["bankofamerica.com", "Bank of America"],
  ["bbc.co.uk", "BBC"],
  ["bbc.com", "BBC"],
  ["bestbuy.com", "Best Buy"],
  ["bing.com", "Bing"],
  ["blogger.com", "Blogger"],
  ["bloomberg.com", "Bloomberg"],
  ["bluehost.com", "Bluehost"],
  ["booking.com", "Booking.com"],
  ["bscscan.com", "BscScan"],
  ["businessinsider.com", "Insider"],
  ["ca.gov", "California State Portal"],
  ["canada.ca", "Government of Canada"],
  ["canva.com", "Canva"],
  ["capitalone.com", "Capital One"],
  ["cdc.gov", "CDC.gov"],
  ["chase.com", "Chase"],
  ["chess.com", "Chess.com"],
  ["citi.com", "Citi.com"],
  ["cj.com", "CJ Affiliate"],
  ["cnbc.com", "CNBC"],
  ["cnet.com", "CNET"],
  ["cnn.com", "CNN"],
  ["cnnindonesia.com", "CNN Indonesia"],
  ["coingecko.com", "CoinGecko"],
  ["coinmarketcap.com", "CoinMarketCap"],
  ["constantcontact.com", "Constant Contact"],
  ["coursera.org", "Coursera"],
  ["cowin.gov.in", "CoWIN"],
  ["craigslist.org", "Craigslist"],
  ["dailymail.co.uk", "Daily Mail"],
  ["dailymotion.com", "Dailymotion"],
  ["deepl.com", "DeepL"],
  ["dell.com", "Dell"],
  ["discord.com", "Discord"],
  ["disneyplus.com", "Disney+"],
  ["docs.google.com", "Google Docs"],
  ["docusign.net", "DocuSign"],
  ["drive.google.com", "Google Drive"],
  ["dropbox.com", "Dropbox"],
  ["duckduckgo.com", "DuckDuckGo"],
  ["ebay.co.uk", "eBay"],
  ["ebay.com", "eBay"],
  ["ebay.de", "eBay"],
  ["espn.com", "ESPN"],
  ["etherscan.io", "Etherscan"],
  ["etrade.com", "E*TRADE"],
  ["etsy.com", "Etsy"],
  ["evernote.com", "Evernote"],
  ["expedia.com", "Expedia"],
  ["facebook.com", "Facebook"],
  ["fandom.com", "Fandom"],
  ["fast.com", "Fast.com"],
  ["fedex.com", "FedEx"],
  ["feedly.com", "Feedly"],
  ["fidelity.com", "Fidelity"],
  ["fiverr.com", "Fiverr"],
  ["flickr.com", "Flickr"],
  ["flipkart.com", "Flipkart"],
  ["force.com", "Salesforce"],
  ["foxnews.com", "Fox News"],
  ["freshdesk.com", "Freshdesk"],
  ["geeksforgeeks.org", "GeeksforGeeks"],
  ["github.com", "GitHub"],
  ["glassdoor.com", "Glassdoor"],
  ["gmail.com", "Gmail"],
  ["godaddy.com", "GoDaddy"],
  ["goodreads.com", "Goodreads"],
  ["google.az", "Google"],
  ["google.ca", "Google"],
  ["google.cn", "Google"],
  ["google.co.id", "Google"],
  ["google.co.in", "Google"],
  ["google.co.jp", "Google"],
  ["google.co.th", "Google"],
  ["google.co.uk", "Google"],
  ["google.com", "Google"],
  ["google.com.ar", "Google"],
  ["google.com.au", "Google"],
  ["google.com.br", "Google"],
  ["google.com.eg", "Google"],
  ["google.com.hk", "Google"],
  ["google.com.mx", "Google"],
  ["google.com.sa", "Google"],
  ["google.com.sg", "Google"],
  ["google.com.tr", "Google"],
  ["google.com.tw", "Google"],
  ["google.de", "Google"],
  ["google.es", "Google"],
  ["google.fr", "Google"],
  ["google.it", "Google"],
  ["google.pl", "Google"],
  ["google.ru", "Google"],
  ["googlevideo.com", "Google Video"],
  ["grammarly.com", "Grammarly"],
  ["hbomax.com", "HBO Max"],
  ["healthline.com", "Healthline"],
  ["homedepot.com", "The Home Depot"],
  ["hootsuite.com", "Hootsuite"],
  ["hostgator.com", "HostGator"],
  ["hotstar.com", "Hotstar"],
  ["hp.com", "HP"],
  ["hulu.com", "Hulu"],
  ["icicibank.com", "ICICI Bank"],
  ["ikea.com", "IKEA"],
  ["ilovepdf.com", "iLovePDF"],
  ["imdb.com", "IMDb"],
  ["imgur.com", "Imgur"],
  ["indeed.com", "Indeed"],
  ["indiamart.com", "IndiaMART"],
  ["indiatimes.com", "Indiatimes"],
  ["instagram.com", "Instagram"],
  ["instructure.com", "Instructure"],
  ["intuit.com", "Intuit"],
  ["investing.com", "Investing.com"],
  ["iqbroker.com", "IQ Option"],
  ["irs.gov", "IRS.gov"],
  ["istockphoto.com", "iStock"],
  ["japanpost.jp", "Japan Post"],
  ["kayak.com ", "Kayak"],
  ["linkedin.com", "LinkedIn"],
  ["linktr.ee", "Linktree"],
  ["live.com", "Live"],
  ["loom.com", "Loom"],
  ["mail.google.com", "Gmail"],
  ["mailchimp.com", "Mailchimp"],
  ["manage.wix.com", "Wix"],
  ["maps.google.com", "Google Maps"],
  ["marca.com", "MARCA"],
  ["mediafire.com", "MediaFire"],
  ["mercadolibre.com.mx", "Mercado Libre"],
  ["mercadolivre.com.br", "Mercado Livre"],
  ["mercari.com", "Mercari"],
  ["microsoft.com", "Microsoft"],
  ["mlb.com", "MLB.com"],
  ["moneycontrol.com", "moneycontrol.com"],
  ["mozilla.org", "Mozilla"],
  ["msn.com", "MSN"],
  ["myshopify.com", "Shopify"],
  ["myworkdayjobs.com", "Workday"],
  ["naukri.com", "Naukri.com"],
  ["ndtv.com", "NDTV.com"],
  ["netflix.com", "Netflix"],
  ["nih.gov", "National Institutes of Health (NIH)"],
  ["nike.com", "Nike"],
  ["nordstrom.com", "Nordstrom"],
  ["notion.so", "Notion"],
  ["nypost.com", "New York Post"],
  ["nytimes.com", "New York Times"],
  ["office.com", "Office"],
  ["office365.com", "Office 365"],
  ["olympics.com", "Olympics"],
  ["onlinesbi.com", "State Bank of India"],
  ["orange.fr", "Orange"],
  ["patreon.com", "Patreon"],
  ["paypal.com", "PayPal"],
  ["pinterest.com", "Pinterest"],
  ["primevideo.com", "Prime Video"],
  ["quora.com", "Quora"],
  ["rakuten.co.jp", "Rakuten"],
  ["rakuten.com", "Rakuten"],
  ["realtor.com", "Realtor.com"],
  ["redd.it", "Reddit"],
  ["reddit.com", "Reddit"],
  ["redfin.com", "Redfin"],
  ["researchgate.net", "ResearchGate"],
  ["reuters.com", "Reuters"],
  ["reverso.net", "Reverso"],
  ["roblox.com", "Roblox"],
  ["rt.com", "RT"],
  ["salesforce.com", "Salesforce"],
  ["samsung.com", "Samsung"],
  ["scribd.com", "Scribd"],
  ["sheets.google.com", "Google Sheets"],
  ["shein.com", "Shein"],
  ["shutterstock.com", "Shutterstock"],
  ["skype.com", "Skype"],
  ["slides.google.com", "Google Slides"],
  ["slideshare.net", "SlideShare"],
  ["soundcloud.com", "SoundCloud"],
  ["speedtest.net", "Speedtest"],
  ["spotify.com", "Spotify"],
  ["squarespace.com", "Squarespace"],
  ["stackexchange.com", "Stack Exchange"],
  ["stackoverflow.com", "Stack Overflow"],
  ["steampowered.com", "Steam"],
  ["taboola.com", "Taboola.com"],
  ["target.com", "Target"],
  ["td.com", "TD Bank"],
  ["telegram.org", "Telegram"],
  ["theguardian.com", "The Guardian"],
  ["tiktok.com", "TikTok"],
  ["tmall.com", "Tmall"],
  ["tokopedia.com", "Tokopedia"],
  ["trello.com", "Trello"],
  ["tripadvisor.com", "Tripadvisor"],
  ["trustpilot.com", "Trustpilot"],
  ["twitch.tv", "Twitch"],
  ["twitter.com", "Twitter"],
  ["udemy.com", "Udemy"],
  ["unsplash.com", "Unsplash"],
  ["ups.com", "UPS"],
  ["upwork.com", "Upwork"],
  ["usps.com", "USPS"],
  ["vimeo.com", "Vimeo"],
  ["w3schools.com", "W3Schools"],
  ["walmart.com", "Walmart"],
  ["washingtonpost.com", "Washington Post"],
  ["wayfair.com", "Wayfair"],
  ["weather.com", "The Weather Channel"],
  ["webmd.com", "WebMD"],
  ["wellsfargo.com", "Wells Fargo"],
  ["wetransfer.com", "WeTransfer"],
  ["whatsapp.com", "WhatsApp"],
  ["wikihow.com", "wikiHow"],
  ["wikimedia.org", "Wikimedia Commons"],
  ["wikipedia.org", "Wikipedia"],
  ["wildberries.ru", "Wildberries"],
  ["wordpress.org", "WordPress.org"],
  ["worldometers.info", "Worldometer"],
  ["wsj.com", "Wall Street Journal"],
  ["xfinity.com", "Xfinity"],
  ["y2mate.com", "Y2mate"],
  ["yahoo.co.jp", "Yahoo Japan"],
  ["yahoo.com", "Yahoo"],
  ["yandex.ru", "Yandex"],
  ["yelp.com", "Yelp"],
  ["youtube.com", "YouTube"],
  ["zendesk.com", "Zendesk"],
  ["zerodha.com", "Zerodha"],
  ["zillow.com", "Zillow"],
  ["zoom.us", "Zoom"],
]);

/**
 * Maps the domains from CUSTOM_NAMES to a regex that matches a URL ending with
 * that domain, meaning the regex also captures potential subdomains.
 */
XPCOMUtils.defineLazyGetter(this, "CUSTOM_NAMES_REGEX", () => {
  let regexMap = new Map();
  for (let suffix of CUSTOM_NAMES.keys()) {
    let reg = new RegExp(`^(.+\.)?${suffix}$`);
    regexMap.set(suffix, reg);
  }
  return regexMap;
});

/**
 * A class that exposes a static method to return a site's "common name". This
 * is ideally the site's brand name with appropriate spacing and capitalization.
 * For example, the common name for stackoverflow.com is "Stack Overflow". If
 * a common name cannot be fetched, the origin is returned with its TLD
 * stripped, e.g. "stackoverflow".
 */
class CommonNames {
  /**
   * Returns a snapshot's common name.
   *
   * @param {Snapshot} snapshot
   *   The snapshot for which to fetch a common name. See Snapshots.jsm for a
   *   definition.
   * @returns {string} The snapshot's common name.
   */
  static getName(snapshot) {
    let commonName = snapshot.pageData.get(PageDataCollector.DATA_TYPE.GENERAL)
      ?.site_name;
    if (commonName) {
      return commonName;
    }

    let url = new URL(snapshot.url);
    let longest = null;
    for (let suffix of CUSTOM_NAMES.keys()) {
      let reg = CUSTOM_NAMES_REGEX.get(suffix);
      if (reg.test(url.hostname)) {
        longest = !longest || longest.length < suffix.length ? suffix : longest;
      }
    }

    return longest ? CUSTOM_NAMES.get(longest) : shortURL({ url });
  }
}
