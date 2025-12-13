import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import panelSidebarSvg from "../features/assets/panelsidebar.svg";
import workspacesSvg from "../features/assets/workspaces.svg";
import pwaSvg from "../features/assets/pwa.svg";
import mouseGestureSvg from "../features/assets/mousegesture.svg";
import gearImage from "../hub/Floorp_Icon_Gear_Gradient.png";
import headerCover from "./header-cover.webp";
import {
  ArrowRight,
  ChevronDown,
  Github,
  Globe,
  Languages,
  MessageCircle,
  Twitter,
} from "lucide-react";
import {
  getLocaleData,
  getNativeNames,
  installLangPack,
  setAppLocale,
} from "../localization/dataManager.ts";
import type { LangPack, LocaleData } from "../localization/type.ts";

function toStringArray(value: unknown): string[] {
  if (Array.isArray(value)) {
    return value.filter((item): item is string =>
      typeof item === "string" && item.trim().length > 0
    );
  }

  if (typeof value === "string") {
    return value.trim().length > 0 ? [value] : [];
  }

  if (value && typeof value === "object") {
    return Object.values(value).filter((item): item is string =>
      typeof item === "string" && item.trim().length > 0
    );
  }

  return [];
}

export default function WhatsNewPage() {
  const { t, i18n } = useTranslation();
  const version = (() => {
    try {
      const params = new URLSearchParams(globalThis.location.search);
      return params.get("upgrade") ?? "12";
    } catch {
      return "12";
    }
  })();

  useEffect(() => {
    const previousTitle = globalThis.document.title;
    const nextTitle = t("whatsNew.documentTitle", {
      version,
      defaultValue: `Floorp – What's New`,
    });
    globalThis.document.title = nextTitle;

    return () => {
      globalThis.document.title = previousTitle;
    };
  }, [i18n.language, t, version]);

  // Localization state
  const [localeData, setlocaleData] = useState<LocaleData | null>(null);
  const [loading, setLoading] = useState(true);
  const [selectedLocale, setSelectedLocale] = useState<string>("");
  const [installingLanguage, setInstallingLanguage] = useState(false);
  const [nativeLanguageNames, setNativeLanguageNames] = useState<
    Record<string, string>
  >({});
  const [dropdownDirection, setDropdownDirection] = useState<
    "dropdown-bottom" | "dropdown-top"
  >("dropdown-bottom");

  const closePage = () => {
    globalThis.open("about:newtab", "_blank");
    setTimeout(() => globalThis.close(), 50);
  };

  // Localization functions
  useEffect(() => {
    // Add a small delay to prevent blocking the main page render
    const timer = setTimeout(() => {
      fetchLocaleInfo();
    }, 500);

    return () => clearTimeout(timer);
  }, []);

  useEffect(() => {
    if (localeData && localeData.availableLocales.length > 0) {
      fetchNativeNames(localeData.availableLocales);
    }
  }, [localeData]);

  // Auto-apply system language for new users (no upgrade parameter)
  useEffect(() => {
    // Only auto-apply if there's no upgrade parameter (new installation)
    const params = new URLSearchParams(globalThis.location.search);
    const hasUpgradeParam = params.has("upgrade");

    if (
      !hasUpgradeParam &&
      localeData &&
      localeData.localeInfo.systemLocale.language !==
        localeData.localeInfo.appLocaleRaw &&
      !installingLanguage
    ) {
      const applySystemLanguage = async () => {
        try {
          await handleLocaleChange(localeData.localeInfo.systemLocale.language);
        } catch (error) {
          console.error("Failed to auto-apply system language:", error);
        }
      };

      // Add a small delay to ensure everything is initialized
      const timer = setTimeout(applySystemLanguage, 1000);
      return () => clearTimeout(timer);
    }
  }, [localeData, installingLanguage]);

  useEffect(() => {
    const checkDropdownDirection = () => {
      const dropdownElement = document.querySelector(".language-dropdown");
      if (dropdownElement) {
        const rect = dropdownElement.getBoundingClientRect();
        const windowHeight = globalThis.innerHeight;
        const dropdownHeight = 240;

        if (rect.bottom + dropdownHeight > windowHeight) {
          setDropdownDirection("dropdown-top");
        } else {
          setDropdownDirection("dropdown-bottom");
        }
      }
    };

    checkDropdownDirection();
    globalThis.addEventListener("resize", checkDropdownDirection);

    return () => {
      globalThis.removeEventListener("resize", checkDropdownDirection);
    };
  }, []);

  const fetchNativeNames = async (availableLocales: LangPack[]) => {
    try {
      const locales = availableLocales.map((langPack) =>
        getLocaleFromLangPack(langPack)
      );

      // Add timeout for native names
      const timeoutPromise = new Promise<never>((_, reject) =>
        setTimeout(() => reject(new Error("Timeout")), 2000)
      );

      const nativeNames = await Promise.race([
        getNativeNames(locales),
        timeoutPromise,
      ]) as string[];
      const namesMap: Record<string, string> = {};
      locales.forEach((locale, index) => {
        namesMap[locale] = nativeNames[index] || locale;
      });
      setNativeLanguageNames(namesMap);
    } catch (error) {
      console.error("Failed to get native language name:", error);
      // Fallback to using locale codes as names
      const locales = availableLocales.map((langPack) =>
        getLocaleFromLangPack(langPack)
      );
      const fallbackMap: Record<string, string> = {};
      locales.forEach((locale) => {
        fallbackMap[locale] = locale;
      });
      setNativeLanguageNames(fallbackMap);
    }
  };

  const fetchLocaleInfo = async () => {
    try {
      setLoading(true);

      // Add timeout to prevent infinite loading
      const timeoutPromise = new Promise<never>((_, reject) =>
        setTimeout(() => reject(new Error("Timeout")), 3000)
      );

      const data = await Promise.race([
        getLocaleData(),
        timeoutPromise,
      ]) as LocaleData;
      setlocaleData(data);
      setSelectedLocale(data.localeInfo.appLocaleRaw);
    } catch (error) {
      console.error("Failed to get language information:", error);
      // Set loading to false and continue without language settings
      setlocaleData(null);
    } finally {
      setLoading(false);
    }
  };

  const getLocaleFromLangPack = (langPack: LangPack): string => {
    return langPack.target_locale;
  };

  const handleLocaleChange = async (locale: string) => {
    if (installingLanguage || locale === selectedLocale) {
      return;
    }

    try {
      setInstallingLanguage(true);

      if (!localeData?.installedLocales.includes(locale)) {
        const targetLangPack = localeData?.availableLocales.find((langPack) =>
          getLocaleFromLangPack(langPack) === locale
        );
        await installLangPack(targetLangPack!);
      }

      await setAppLocale(locale);
      await i18n.changeLanguage(locale);

      setInstallingLanguage(false);
      setSelectedLocale(locale);
    } catch (error) {
      console.error("Failed to change locale:", error);
      setInstallingLanguage(false);
    }
  };

  const getLanguageName = (langCode: string) => {
    if (nativeLanguageNames[langCode]) {
      return nativeLanguageNames[langCode];
    }
    return langCode;
  };

  const getSortedLanguages = () => {
    if (!localeData) return [];
    return [...localeData.availableLocales].sort((a, b) => {
      const localeA = getLocaleFromLangPack(a);
      const localeB = getLocaleFromLangPack(b);
      const nameA = getLanguageName(localeA);
      const nameB = getLanguageName(localeB);
      return nameA.localeCompare(nameB);
    });
  };

  return (
    <div className="min-h-screen flex flex-col">
      <header className="relative overflow-hidden bg-base-100">
        <div
          className="relative m-5 rounded-2xl"
          style={{
            backgroundImage: `url(${headerCover})`,
            backgroundSize: "cover",
            backgroundPosition: "center",
          }}
        >
          <div className="container mx-auto px-6 py-32 relative">
            <div className="text-center">
              <h1 className="text-5xl md:text-5xl font-bold text-gray-200 mb-8">
                {t("whatsNew.title", { version })}
              </h1>
              <p className="text-lg md:text-xl text-gray-100/90">
                {t("whatsNew.subtitle")}
              </p>
            </div>
          </div>
        </div>
      </header>

      {/* Language Selection Section */}
      <section className="py-8">
        <div className="container mx-auto max-w-6xl px-6">
          {loading
            ? (
              <div className="flex justify-center">
                <div className="loading loading-spinner loading-lg"></div>
              </div>
            )
            : !localeData
            ? (
              <div className="card bg-base-100 shadow-2xl">
                <div className="card-body p-8 text-center">
                  <div className="inline-flex items-center justify-center w-20 h-20 bg-primary/10 rounded-full mb-4">
                    <Globe size={40} className="text-primary" />
                  </div>
                  <h2 className="text-3xl font-bold mb-2">
                    {t("localizationPage.languageSettings")}
                  </h2>
                  <p className="text-base-content/70 mb-4">
                    Language settings are temporarily unavailable. You can
                    continue with the current language.
                  </p>
                  <button
                    type="button"
                    className="btn btn-primary"
                    onClick={() => fetchLocaleInfo()}
                  >
                    Try Again
                  </button>
                </div>
              </div>
            )
            : (
              <div className="card bg-base-100 shadow-xl">
                <div className="card-body p-6">
                  {/* Icon and Title */}
                  <div className="flex items-center gap-3 shrink-0">
                    <div className="inline-flex items-center justify-center w-12 h-12 bg-primary/10 rounded-full">
                      <Globe size={24} className="text-primary" />
                    </div>
                    <h3 className="text-lg font-bold whitespace-nowrap">
                      {t("localizationPage.languageSettings")}
                    </h3>
                  </div>
                  <div className="flex flex-wrap lg:flex-nowrap gap-4 items-center">
                    {/* Current Language Info */}
                    <div className="bg-base-200 p-3 rounded-lg grow min-w-0">
                      <h4 className="font-semibold mb-2 text-xs">
                        {t("localizationPage.currentLanguageInfo")}
                      </h4>
                      <div className="flex gap-4">
                        <div className="min-w-0 flex-1">
                          <div className="text-xs opacity-70">
                            {t("localizationPage.systemLanguage")}
                          </div>
                          <div className="font-medium text-sm truncate">
                            {localeData.localeInfo.displayNames
                              ?.systemLanguage ||
                              localeData.localeInfo.systemLocaleRaw}
                          </div>
                        </div>
                        <div className="min-w-0 flex-1">
                          <div className="text-xs opacity-70">
                            {t("localizationPage.floorpLanguage")}
                          </div>
                          <div className="font-medium text-sm truncate">
                            {getLanguageName(selectedLocale)}
                          </div>
                        </div>
                      </div>
                    </div>

                    {/* System Language Button */}
                    <div className="shrink-0">
                      <button
                        type="button"
                        className="btn btn-primary btn-sm whitespace-nowrap"
                        onClick={() =>
                          handleLocaleChange(
                            localeData.localeInfo.systemLocale.language,
                          )}
                        disabled={installingLanguage ||
                          localeData.localeInfo.systemLocaleRaw ===
                            selectedLocale}
                      >
                        <Globe size={14} className="mr-2" />
                        {t("localizationPage.useSystemLanguage")}
                      </button>
                    </div>

                    {/* Language Dropdown */}
                    <div className="shrink-0 min-w-48">
                      <div
                        className={`dropdown ${dropdownDirection} w-full language-dropdown`}
                      >
                        <label
                          tabIndex={0}
                          className="btn btn-outline btn-sm w-full flex justify-between items-center"
                        >
                          <span className="flex items-center">
                            <Languages size={14} className="mr-2" />
                            <span className="truncate">
                              {getLanguageName(selectedLocale)}
                            </span>
                          </span>
                          <ChevronDown size={14} />
                        </label>
                        <div
                          tabIndex={0}
                          className="dropdown-content menu p-2 shadow bg-base-100 rounded-box w-full overflow-y-auto flex flex-col flex-nowrap max-h-60 z-50"
                        >
                          {getSortedLanguages().map((langPack) => {
                            const locale = getLocaleFromLangPack(langPack);
                            if (locale === selectedLocale) {
                              return null;
                            }
                            return (
                              <li
                                key={locale}
                                onClick={() => {
                                  handleLocaleChange(locale);
                                  if (
                                    document.activeElement instanceof
                                      HTMLElement
                                  ) {
                                    document.activeElement.blur();
                                  }
                                }}
                                className="w-full"
                              >
                                <a
                                  className={selectedLocale === locale
                                    ? "active"
                                    : ""}
                                >
                                  <span className="truncate">
                                    {getLanguageName(locale)}
                                  </span>
                                </a>
                              </li>
                            );
                          })}
                        </div>
                      </div>

                      {installingLanguage && (
                        <div className="flex justify-center items-center mt-2">
                          <div className="loading loading-spinner loading-xs mr-2">
                          </div>
                          <span className="text-xs">
                            Changing language...
                          </span>
                        </div>
                      )}
                    </div>
                  </div>
                </div>
              </div>
            )}
        </div>
      </section>

      <main className="bg-base-100 flex-1">
        <div className="container mx-auto max-w-6xl px-6">
          <FeatureSection
            tag={t("whatsNew.tags.settings")}
            title={t("hubPage.sectionTitle")}
            description={t("hubPage.description")}
            image={gearImage}
            bullets={[
              t("hubPage.features.dedicated"),
              t("hubPage.features.accessible"),
              t("hubPage.features.centralized"),
            ]}
          />

          <FeatureSection
            tag={t("whatsNew.tags.productivity")}
            title={t("featuresPage.panelSidebar.title")}
            description={t("featuresPage.panelSidebar.description")}
            image={panelSidebarSvg}
            bullets={toStringArray(
              t("featuresPage.panelSidebar.features", {
                returnObjects: true,
              }),
            )}
            reverse
          />

          <FeatureSection
            tag={t("whatsNew.tags.workspaces")}
            title={t("featuresPage.workspaces.title")}
            description={t("featuresPage.workspaces.description")}
            image={workspacesSvg}
            bullets={toStringArray(
              t("featuresPage.workspaces.features", {
                returnObjects: true,
              }),
            )}
          />

          <FeatureSection
            tag={t("whatsNew.tags.pwa")}
            title={t("featuresPage.pwa.title")}
            description={t("featuresPage.pwa.description")}
            image={pwaSvg}
            bullets={toStringArray(
              t("featuresPage.pwa.features", {
                returnObjects: true,
              }),
            )}
            reverse
          />

          <FeatureSection
            tag={t("whatsNew.tags.gestures")}
            title={t("featuresPage.mouseGesture.title")}
            description={t("featuresPage.mouseGesture.description")}
            image={mouseGestureSvg}
            bullets={toStringArray(
              t("featuresPage.mouseGesture.features", {
                returnObjects: true,
              }),
            )}
          />

          <div className="py-16 text-center">
            <button
              type="button"
              className="btn btn-primary btn-lg"
              onClick={closePage}
            >
              <ArrowRight className="mr-2" size={20} />{" "}
              {t("finishPage.getStarted")}
            </button>
          </div>
        </div>
      </main>

      {/* Footer */}
      <footer className="bg-gray-800 text-gray-300 py-12">
        <div className="container mx-auto max-w-6xl px-6">
          <div className="flex flex-col lg:flex-row justify-between items-start lg:items-center gap-8">
            {/* Social Media Links */}
            <div className="flex flex-col gap-4">
              <h3 className="text-white font-medium">
                {t("whatsNew.footer.follow")}
              </h3>
              <div className="flex gap-4">
                <a
                  href="https://github.com/Floorp-Projects/Floorp"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg hover:bg-gray-700 transition-colors"
                  aria-label="GitHub"
                >
                  <Github size={24} />
                </a>
                <a
                  href="https://twitter.com/floorp_browser"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg hover:bg-gray-700 transition-colors"
                  aria-label="X (Twitter)"
                >
                  <Twitter size={24} />
                </a>
                <a
                  href="https://floorp.app/discord"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg hover:bg-gray-700 transition-colors"
                  aria-label="Discord"
                >
                  <MessageCircle size={24} />
                </a>
              </div>
            </div>

            {/* Brand and Links */}
            <div className="flex flex-col lg:flex-row items-start lg:items-center gap-6">
              <div className="flex items-center gap-2">
                <img
                  src="chrome://branding/content/icon128.png"
                  alt="Floorp"
                  className="w-6 h-6"
                />
                <span className="text-white font-medium">Floorp</span>
              </div>

              <div className="flex flex-wrap gap-6 text-sm">
                <a
                  href="https://floorp.app/privacy"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-white transition-colors"
                >
                  {t("whatsNew.footer.privacy")}
                </a>
                <a
                  href="https://docs.floorp.app"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-white transition-colors"
                >
                  {t("whatsNew.footer.about")}
                </a>
              </div>
            </div>
          </div>

          <div className="mt-8 pt-8 border-t border-gray-700">
            <p className="text-center text-sm text-gray-400">
              {t("whatsNew.footer.copyright", {
                year: new Date().getFullYear(),
              })}
            </p>
          </div>
        </div>
      </footer>
    </div>
  );
}

function FeatureSection(props: {
  tag: string;
  title: string;
  description: string;
  image: string;
  bullets: string[];
  reverse?: boolean;
}) {
  return (
    <section className="py-16">
      <div
        className={`flex flex-col lg:flex-row gap-12 items-center ${
          props.reverse ? "lg:flex-row-reverse" : ""
        }`}
      >
        <div className="lg:w-1/2 space-y-6">
          <div>
            <div className="inline-block bg-primary text-primary-content text-sm font-medium px-3 py-1 rounded-full mb-4">
              {props.tag}
            </div>
            <h2 className="text-3xl lg:text-4xl font-bold mb-4">
              {props.title}
            </h2>
            <p className="text-lg text-base-content/80 leading-relaxed mb-6">
              {props.description}
            </p>
          </div>

          {props.bullets.length > 0 && (
            <div className="space-y-3">
              <ol className="space-y-2">
                {props.bullets.map((feature, index) => (
                  <li key={index} className="flex items-start gap-3">
                    <span className="shrink-0 w-6 h-6 bg-primary text-primary-content rounded-full flex items-center justify-center text-sm font-medium">
                      {index + 1}
                    </span>
                    <span className="text-base-content/80">{feature}</span>
                  </li>
                ))}
              </ol>
            </div>
          )}
        </div>

        <div className="lg:w-1/2">
          <div className="bg-base-200 rounded-2xl p-8 shadow-lg">
            <img
              src={props.image}
              alt={props.title}
              className="w-full h-auto object-contain max-h-80"
            />
          </div>
        </div>
      </div>
    </section>
  );
}
