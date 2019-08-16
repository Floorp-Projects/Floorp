/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { mount } = require("enzyme");
const { REPS } = require("../rep");
const { Rep } = REPS;
const { getGripLengthBubbleText } = require("./test-helpers");

const renderRep = (string, props) =>
  mount(
    Rep({
      object: string,
      ...props,
    })
  );

const testLinkClick = (link, openLink, url) => {
  let syntheticEvent;
  const preventDefault = jest.fn().mockImplementation(function() {
    // This refers to the event object for which preventDefault is called (in
    // this case it is the syntheticEvent that is passed to onClick and
    // consequently to openLink).
    syntheticEvent = this;
  });

  link.simulate("click", { preventDefault });
  // Prevent defaults behavior on click
  expect(preventDefault).toHaveBeenCalled();
  expect(openLink).toHaveBeenCalledWith(url, syntheticEvent);
};

describe("test String with URL", () => {
  it("renders a URL", () => {
    const url = "http://example.com";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a href when openLink isn't defined", () => {
    const url = "http://example.com";
    const element = renderRep(url, { useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(null);
    expect(link.prop("title")).toBe(url);
  });

  it("renders a href when no openLink but isInContentPage is true", () => {
    const url = "http://example.com";
    const element = renderRep(url, { useQuotes: false, isInContentPage: true });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);
  });

  it("renders a simple quoted URL", () => {
    const url = "http://example.com";
    const string = `'${url}'`;
    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: false });
    expect(element.text()).toEqual(string);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a double quoted URL", () => {
    const url = "http://example.com";
    const string = `"${url}"`;
    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: false });
    expect(element.text()).toEqual(string);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a quoted URL when useQuotes is true", () => {
    const url = "http://example.com";
    const string = `"${url}"`;
    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: true });
    expect(element.text()).toEqual(`"\\"${url}\\""`);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a simple https URL", () => {
    const url = "https://example.com";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a simple http URL with one slash", () => {
    const url = "https:/example.com";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a URL with port", () => {
    const url = "https://example.com:443";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a URL with non-empty path", () => {
    const url = "http://example.com/foo";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a URL when surrounded by non-URL tokens", () => {
    const url = "http://example.com";
    const string = `foo ${url} bar`;
    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: false });
    expect(element.text()).toEqual(string);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a URL and whitespace is be preserved", () => {
    const url = "http://example.com";
    const string = `foo\n${url}\nbar\n`;
    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: false });
    expect(element.text()).toEqual(string);

    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders multiple URLs", () => {
    const url1 = "http://example.com";
    const url2 = "https://example.com/foo";
    const string = `${url1} ${url2}`;
    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: false });
    expect(element.text()).toEqual(string);
    const links = element.find("a");
    expect(links).toHaveLength(2);

    const firstLink = links.at(0);
    expect(firstLink.prop("href")).toBe(url1);
    expect(firstLink.prop("title")).toBe(url1);
    testLinkClick(firstLink, openLink, url1);

    const secondLink = links.at(1);
    expect(secondLink.prop("href")).toBe(url2);
    expect(secondLink.prop("title")).toBe(url2);
    testLinkClick(secondLink, openLink, url2);
  });

  it("renders multiple URLs with various spacing", () => {
    const url1 = "http://example.com";
    const url2 = "https://example.com/foo";
    const string = `  ${url1}      ${url2}  ${url2}     ${url1}    `;
    const element = renderRep(string, { useQuotes: false });
    expect(element.text()).toEqual(string);
    const links = element.find("a");
    expect(links).toHaveLength(4);
  });

  it("renders a cropped URL", () => {
    const url = "http://example.com";
    const openLink = jest.fn();
    const element = renderRep(url, {
      openLink,
      useQuotes: false,
      cropLimit: 15,
    });

    expect(element.text()).toEqual("http://…ple.com");
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a non-cropped URL", () => {
    const url = "http://example.com/foobarbaz";
    const openLink = jest.fn();
    const element = renderRep(url, {
      openLink,
      useQuotes: false,
      cropLimit: 50,
    });

    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders URL on an open string", () => {
    const url = "http://example.com";
    const openLink = jest.fn();
    const element = renderRep(url, {
      openLink,
      useQuotes: false,
      member: {
        open: true,
      },
    });

    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders URLs with a stripped string between", () => {
    const text = "- http://example.fr --- http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 41,
    });

    expect(element.text()).toEqual("- http://example.fr … http://example.us -");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example.fr");
    expect(linkFr.prop("title")).toBe("http://example.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe("http://example.us");
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders URLs with a cropped string between", () => {
    const text = "- http://example.fr ---- http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 42,
    });

    expect(element.text()).toEqual(
      "- http://example.fr -…- http://example.us -"
    );
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example.fr");
    expect(linkFr.prop("title")).toBe("http://example.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe("http://example.us");
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders successive cropped URLs, 1 at the start, 1 at the end", () => {
    const text = "- http://example-long.fr http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20,
    });

    expect(element.text()).toEqual("- http://e…ample.us -");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example-long.fr");
    expect(linkFr.prop("title")).toBe("http://example-long.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe("http://example.us");
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders successive URLs, one cropped in the middle", () => {
    const text =
      "- http://example-long.fr http://example.com http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 60,
    });

    expect(element.text()).toEqual(
      "- http://example-long.fr http:…xample.com http://example.us -"
    );
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example-long.fr");
    expect(linkFr.prop("title")).toBe("http://example-long.fr");

    const linkCom = element.find("a").at(1);
    expect(linkCom.prop("href")).toBe("http://example.com");
    expect(linkCom.prop("title")).toBe("http://example.com");

    const linkUs = element.find("a").at(2);
    expect(linkUs.prop("href")).toBe("http://example.us");
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders successive cropped URLs with cropped elements between", () => {
    const text =
      "- http://example.fr test http://example.es test http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20,
    });

    expect(element.text()).toEqual("- http://e…ample.us -");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example.fr");
    expect(linkFr.prop("title")).toBe("http://example.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe("http://example.us");
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders a cropped URL followed by a cropped string", () => {
    const text = "http://example.fr abcdefghijkl";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20,
    });

    expect(element.text()).toEqual("http://exa…cdefghijkl");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example.fr");
    expect(linkFr.prop("title")).toBe("http://example.fr");
  });

  it("renders a cropped string followed by a cropped URL", () => {
    const text = "abcdefghijkl stripped http://example.fr ";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20,
    });

    expect(element.text()).toEqual("abcdefghij…xample.fr ");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example.fr");
    expect(linkFr.prop("title")).toBe("http://example.fr");
  });

  it("renders URLs without unrelated characters", () => {
    const text =
      "global(http://example.com) and local(http://example.us)" +
      " and maybe https://example.fr, “https://example.cz“, https://example.es?";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
    });

    expect(element.text()).toEqual(text);
    const linkCom = element.find("a").at(0);
    expect(linkCom.prop("href")).toBe("http://example.com");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe("http://example.us");

    const linkFr = element.find("a").at(2);
    expect(linkFr.prop("href")).toBe("https://example.fr");

    const linkCz = element.find("a").at(3);
    expect(linkCz.prop("href")).toBe("https://example.cz");

    const linkEs = element.find("a").at(4);
    expect(linkEs.prop("href")).toBe("https://example.es");
  });

  it("renders a cropped URL with urlCropLimit", () => {
    const xyzUrl = "http://xyz.com/abcdefghijklmnopqrst";
    const text = `${xyzUrl} is the best`;
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      urlCropLimit: 20,
    });

    expect(element.text()).toEqual("http://xyz…klmnopqrst is the best");
    const link = element.find("a").at(0);
    expect(link.prop("href")).toBe(xyzUrl);
    expect(link.prop("title")).toBe(xyzUrl);
  });

  it("renders multiple cropped URL", () => {
    const xyzUrl = "http://xyz.com/abcdefghijklmnopqrst";
    const abcUrl = "http://abc.com/abcdefghijklmnopqrst";
    const text = `${xyzUrl} is lit, not ${abcUrl}`;
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      urlCropLimit: 20,
    });

    expect(element.text()).toEqual(
      "http://xyz…klmnopqrst is lit, not http://abc…klmnopqrst"
    );

    const links = element.find("a");
    const xyzLink = links.at(0);
    expect(xyzLink.prop("href")).toBe(xyzUrl);
    expect(xyzLink.prop("title")).toBe(xyzUrl);
    const abc = links.at(1);
    expect(abc.prop("href")).toBe(abcUrl);
    expect(abc.prop("title")).toBe(abcUrl);
  });

  it("renders full URL if smaller than cropLimit", () => {
    const xyzUrl = "http://example.com/";

    const openLink = jest.fn();
    const element = renderRep(xyzUrl, {
      openLink,
      useQuotes: false,
      urlCropLimit: 20,
    });

    expect(element.text()).toEqual(xyzUrl);
    const link = element.find("a").at(0);
    expect(link.prop("href")).toBe(xyzUrl);
    expect(link.prop("title")).toBe(xyzUrl);
  });

  it("renders cropped URL followed by cropped string with urlCropLimit", () => {
    const text = "http://example.fr abcdefghijkl";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20,
    });

    expect(element.text()).toEqual("http://exa…cdefghijkl");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe("http://example.fr");
    expect(linkFr.prop("title")).toBe("http://example.fr");
  });

  it("does not render a link if the URL has no scheme", () => {
    const url = "example.com";
    const element = renderRep(url, { useQuotes: false });
    expect(element.text()).toEqual(url);
    expect(element.find("a").exists()).toBeFalsy();
  });

  it("does not render a link if the URL has an invalid scheme", () => {
    const url = "foo://example.com";
    const element = renderRep(url, { useQuotes: false });
    expect(element.text()).toEqual(url);
    expect(element.find("a").exists()).toBeFalsy();
  });

  it("does not render an invalid URL that requires cropping", () => {
    const text =
      "//www.youtubeinmp3.com/download/?video=https://www.youtube.com/watch?v=8vkfsCIfDFc";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 60,
    });
    expect(element.text()).toEqual(
      "//www.youtubeinmp3.com/downloa…outube.com/watch?v=8vkfsCIfDFc"
    );
    expect(element.find("a").exists()).toBeFalsy();
  });

  it("does render a link in a plain array", () => {
    const url = "http://example.com/abcdefghijabcdefghij";
    const string = `${url} some other text`;
    const object = [string];
    const openLink = jest.fn();
    const element = renderRep(object, { openLink, noGrip: true });
    expect(element.text()).toEqual(`[ "${string}" ]`);

    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("does render a link in a grip array", () => {
    const object = require("../stubs/grip-array").get(
      '["http://example.com/abcdefghijabcdefghij some other text"]'
    );
    const length = getGripLengthBubbleText(object);
    const openLink = jest.fn();
    const element = renderRep(object, { openLink });

    const url = "http://example.com/abcdefghijabcdefghij";
    const string = `${url} some other text`;
    expect(element.text()).toEqual(`Array${length} [ "${string}" ]`);

    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("does render a link in a plain object", () => {
    const url = "http://example.com/abcdefghijabcdefghij";
    const string = `${url} some other text`;
    const object = { test: string };
    const openLink = jest.fn();
    const element = renderRep(object, { openLink, noGrip: true });
    expect(element.text()).toEqual(`Object { test: "${string}" }`);

    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("does render a link in a grip object", () => {
    const object = require("../stubs/grip").get(
      '{test: "http://example.com/ some other text"}'
    );
    const openLink = jest.fn();
    const element = renderRep(object, { openLink });

    const url = "http://example.com/";
    const string = `${url} some other text`;
    expect(element.text()).toEqual(`Object { test: "${string}" }`);

    const link = element.find("a");
    expect(link.prop("href")).toBe(url);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("does not render links for js URL", () => {
    const url = "javascript:x=42";
    const string = `${url} some other text`;

    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: false });
    expect(element.text()).toEqual(string);
    const link = element.find("a");
    expect(link.exists()).toBe(false);
  });
});
