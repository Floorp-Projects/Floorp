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
      ...props
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
  expect(preventDefault).toBeCalled();
  expect(openLink).toBeCalledWith(url, syntheticEvent);
};

describe("test String with URL", () => {
  it("renders a URL", () => {
    const url = "http://example.com";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a simple quoted URL", () => {
    const url = "http://example.com";
    const string = `'${url}'`;
    const openLink = jest.fn();
    const element = renderRep(string, { openLink, useQuotes: false });
    expect(element.text()).toEqual(string);
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
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
    expect(link.prop("href")).toBe(undefined);
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
    expect(link.prop("href")).toBe(undefined);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a simple https URL", () => {
    const url = "https://example.com";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a URL with port", () => {
    const url = "https://example.com:443";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a URL with non-empty path", () => {
    const url = "http://example.com/foo";
    const openLink = jest.fn();
    const element = renderRep(url, { openLink, useQuotes: false });
    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
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
    expect(link.prop("href")).toBe(undefined);
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
    expect(link.prop("href")).toBe(undefined);
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
    expect(firstLink.prop("href")).toBe(undefined);
    expect(firstLink.prop("title")).toBe(url1);
    testLinkClick(firstLink, openLink, url1);

    const secondLink = links.at(1);
    expect(secondLink.prop("href")).toBe(undefined);
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
      cropLimit: 15
    });

    expect(element.text()).toEqual("http://…ple.com");
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders a non-cropped URL", () => {
    const url = "http://example.com/foobarbaz";
    const openLink = jest.fn();
    const element = renderRep(url, {
      openLink,
      useQuotes: false,
      cropLimit: 50
    });

    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
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
        open: true
      }
    });

    expect(element.text()).toEqual(url);
    const link = element.find("a");
    expect(link.prop("href")).toBe(undefined);
    expect(link.prop("title")).toBe(url);

    testLinkClick(link, openLink, url);
  });

  it("renders URLs with a stripped string between", () => {
    const text = "- http://example.fr --- http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 41
    });

    expect(element.text()).toEqual("- http://example.fr … http://example.us -");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe(undefined);
    expect(linkFr.prop("title")).toBe("http://example.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe(undefined);
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders URLs with a cropped string between", () => {
    const text = "- http://example.fr ---- http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 42
    });

    expect(element.text()).toEqual(
      "- http://example.fr -…- http://example.us -"
    );
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe(undefined);
    expect(linkFr.prop("title")).toBe("http://example.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe(undefined);
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders successive cropped URLs, 1 at the start, 1 at the end", () => {
    const text = "- http://example-long.fr http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20
    });

    expect(element.text()).toEqual("- http://e…ample.us -");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe(undefined);
    expect(linkFr.prop("title")).toBe("http://example-long.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe(undefined);
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders successive URLs, one cropped in the middle", () => {
    const text =
      "- http://example-long.fr http://example.com http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 60
    });

    expect(element.text()).toEqual(
      "- http://example-long.fr http:…xample.com http://example.us -"
    );
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe(undefined);
    expect(linkFr.prop("title")).toBe("http://example-long.fr");

    const linkCom = element.find("a").at(1);
    expect(linkCom.prop("href")).toBe(undefined);
    expect(linkCom.prop("title")).toBe("http://example.com");

    const linkUs = element.find("a").at(2);
    expect(linkUs.prop("href")).toBe(undefined);
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders successive cropped URLs with cropped elements between", () => {
    const text =
      "- http://example.fr test http://example.fr test http://example.us -";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20
    });

    expect(element.text()).toEqual("- http://e…ample.us -");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe(undefined);
    expect(linkFr.prop("title")).toBe("http://example.fr");

    const linkUs = element.find("a").at(1);
    expect(linkUs.prop("href")).toBe(undefined);
    expect(linkUs.prop("title")).toBe("http://example.us");
  });

  it("renders a cropped URL followed by a cropped string", () => {
    const text = "http://example.fr abcdefghijkl";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20
    });

    expect(element.text()).toEqual("http://exa…cdefghijkl");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe(undefined);
    expect(linkFr.prop("title")).toBe("http://example.fr");
  });

  it("renders a cropped string followed by a cropped URL", () => {
    const text = "abcdefghijkl stripped http://example.fr ";
    const openLink = jest.fn();
    const element = renderRep(text, {
      openLink,
      useQuotes: false,
      cropLimit: 20
    });

    expect(element.text()).toEqual("abcdefghij…xample.fr ");
    const linkFr = element.find("a").at(0);
    expect(linkFr.prop("href")).toBe(undefined);
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
      cropLimit: 60
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
    expect(link.prop("href")).toBe(undefined);
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
    expect(link.prop("href")).toBe(undefined);
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
    expect(link.prop("href")).toBe(undefined);
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
    expect(link.prop("href")).toBe(undefined);
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
