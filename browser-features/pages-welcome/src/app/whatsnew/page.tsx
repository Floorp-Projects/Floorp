import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import panelSidebarSvg from "../features/assets/panelsidebar.svg";
import workspacesSvg from "../features/assets/workspaces.svg";
import pwaSvg from "../features/assets/pwa.svg";
import mouseGestureSvg from "../features/assets/mousegesture.svg";
import gearImage from "../hub/Floorp_Icon_Gear_Gradient.png";
import headerCover from "./header-cover.webp";
import {
  ArrowRight,
  Check,
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
    const isUserLocaleSet = localeData?.localeInfo.isUserLocaleSet;

    if (
      !hasUpgradeParam &&
      localeData &&
      isUserLocaleSet === false &&
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
    <div className="min-h-screen flex flex-col bg-base-100">
      {/* ── Hero ─────────────────────────────────────────── */}
      <header className="relative overflow-hidden">
        <div
          className="hero-gradient relative m-4 sm:m-5 rounded-3xl overflow-hidden"
          style={{
            backgroundImage: `url(${headerCover})`,
            backgroundSize: "cover",
            backgroundPosition: "center",
          }}
        >
          <div className="relative z-10 mx-auto max-w-3xl px-6 py-28 sm:py-36 text-center">
            <p className="mb-4 text-sm font-semibold uppercase tracking-[0.2em] text-white/70">
              Floorp v{version}
            </p>
            <h1
              className="font-bold text-white leading-[1.1]"
              style={{ fontSize: "clamp(2rem, 5vw, 3.25rem)" }}
            >
              {t("whatsNew.title", { version })}
            </h1>
            <p
              className="mx-auto mt-5 max-w-xl text-white/85 leading-relaxed"
              style={{ fontSize: "clamp(1rem, 2vw, 1.25rem)" }}
            >
              {t("whatsNew.subtitle")}
            </p>
          </div>
        </div>
      </header>

      {/* ── Language selector ────────────────────────────── */}
      <section className="py-6 sm:py-8">
        <div className="mx-auto max-w-5xl px-6">
          {loading
            ? (
              <div className="flex justify-center py-6">
                <div className="loading loading-spinner loading-lg" />
              </div>
            )
            : !localeData
            ? (
              <div className="card bg-base-200/60 border border-base-300">
                <div className="card-body items-center text-center gap-4 p-8">
                  <div className="flex h-16 w-16 items-center justify-center rounded-2xl bg-primary/10">
                    <Globe size={32} className="text-primary" />
                  </div>
                  <h2 className="text-2xl font-bold">
                    {t("localizationPage.languageSettings")}
                  </h2>
                  <p className="text-base-content/60 text-sm max-w-md">
                    Language settings are temporarily unavailable. You can
                    continue with the current language.
                  </p>
                  <button
                    type="button"
                    className="btn btn-primary btn-sm mt-2"
                    onClick={() => fetchLocaleInfo()}
                  >
                    Try Again
                  </button>
                </div>
              </div>
            )
            : (
              <div className="flex flex-wrap items-center gap-4 rounded-2xl border border-base-300 bg-base-200/40 p-4 sm:p-5">
                {/* Globe icon + title */}
                <div className="flex items-center gap-3 shrink-0">
                  <div className="flex h-10 w-10 items-center justify-center rounded-xl bg-primary/10">
                    <Globe size={20} className="text-primary" />
                  </div>
                  <div className="leading-tight">
                    <h3 className="text-sm font-bold">
                      {t("localizationPage.languageSettings")}
                    </h3>
                    <p className="text-xs text-base-content/50 mt-0.5">
                      {t("localizationPage.currentLanguageInfo")}
                    </p>
                  </div>
                </div>

                {/* Spacer on lg */}
                <div className="hidden lg:block flex-1" />

                {/* Current locale labels */}
                <div className="flex gap-6 text-sm grow min-w-0 lg:grow-0">
                  <div className="min-w-0">
                    <div className="text-[11px] font-medium text-base-content/50 uppercase tracking-wide">
                      {t("localizationPage.systemLanguage")}
                    </div>
                    <div className="font-semibold truncate">
                      {localeData.localeInfo.displayNames?.systemLanguage ||
                        localeData.localeInfo.systemLocaleRaw}
                    </div>
                  </div>
                  <div className="min-w-0">
                    <div className="text-[11px] font-medium text-base-content/50 uppercase tracking-wide">
                      {t("localizationPage.floorpLanguage")}
                    </div>
                    <div className="font-semibold truncate">
                      {getLanguageName(selectedLocale)}
                    </div>
                  </div>
                </div>

                {/* System lang button */}
                <button
                  type="button"
                  className="btn btn-primary btn-sm shrink-0"
                  onClick={() =>
                    handleLocaleChange(
                      localeData.localeInfo.systemLocale.language,
                    )}
                  disabled={installingLanguage ||
                    localeData.localeInfo.systemLocaleRaw === selectedLocale}
                >
                  <Globe size={14} />
                  {t("localizationPage.useSystemLanguage")}
                </button>

                {/* Language dropdown */}
                <div className="shrink-0 min-w-44">
                  <div
                    className={`dropdown ${dropdownDirection} w-full language-dropdown`}
                  >
                    <label
                      tabIndex={0}
                      className="btn btn-outline btn-sm w-full justify-between"
                    >
                      <span className="flex items-center gap-2">
                        <Languages size={14} />
                        <span className="truncate">
                          {getLanguageName(selectedLocale)}
                        </span>
                      </span>
                      <ChevronDown size={14} />
                    </label>
                    <div
                      tabIndex={0}
                      className="dropdown-content menu p-2 shadow-lg bg-base-100 rounded-xl w-full max-h-60 overflow-y-auto flex flex-col flex-nowrap z-50 border border-base-300"
                    >
                      {getSortedLanguages().map((langPack) => {
                        const locale = getLocaleFromLangPack(langPack);
                        if (locale === selectedLocale) return null;
                        return (
                          <li
                            key={locale}
                            onClick={() => {
                              handleLocaleChange(locale);
                              if (
                                document.activeElement instanceof HTMLElement
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
                    <div className="flex items-center gap-2 mt-2 text-xs text-base-content/60">
                      <div className="loading loading-spinner loading-xs" />
                      Changing language...
                    </div>
                  )}
                </div>
              </div>
            )}
        </div>
      </section>

      {/* ── Feature showcase ─────────────────────────────── */}
      <main className="flex-1">
        <div className="mx-auto max-w-5xl px-6">
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
        </div>

        {/* ── CTA ───────────────────────────────────────── */}
        <div className="mt-12 border-t border-base-300">
          <div className="mx-auto max-w-5xl px-6 py-20 flex flex-col items-center gap-4 text-center">
            <h2
              className="font-bold text-base-content leading-tight"
              style={{ fontSize: "clamp(1.5rem, 3vw, 2.25rem)" }}
            >
              {t("finishPage.getStarted")}
            </h2>
            <p className="text-base-content/60 text-sm max-w-md">
              {t("whatsNew.subtitle")}
            </p>
            <button
              type="button"
              className="btn btn-primary btn-lg mt-2 gap-2"
              onClick={closePage}
            >
              {t("finishPage.getStarted")}
              <ArrowRight size={18} />
            </button>
          </div>
        </div>
      </main>

      {/* ── Footer ──────────────────────────────────────── */}
      <footer className="bg-neutral text-neutral-content py-10">
        <div className="mx-auto max-w-5xl px-6">
          <div className="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-8">
            {/* Brand */}
            <div className="flex items-center gap-3">
              <img
                src="chrome://branding/content/icon128.png"
                alt="Floorp"
                className="w-7 h-7"
              />
              <span className="font-semibold text-lg">Floorp</span>
            </div>

            {/* Links */}
            <div className="flex flex-wrap items-center gap-6 text-sm">
              <a
                href="https://floorp.app/privacy"
                target="_blank"
                rel="noopener noreferrer"
                className="link link-hover"
              >
                {t("whatsNew.footer.privacy")}
              </a>
              <a
                href="https://docs.floorp.app"
                target="_blank"
                rel="noopener noreferrer"
                className="link link-hover"
              >
                {t("whatsNew.footer.about")}
              </a>
            </div>

            {/* Social */}
            <div className="flex items-center gap-1">
              <a
                href="https://github.com/Floorp-Projects/Floorp"
                target="_blank"
                rel="noopener noreferrer"
                className="btn btn-ghost btn-sm btn-square"
                aria-label="GitHub"
              >
                <Github size={18} />
              </a>
              <a
                href="https://twitter.com/floorp_browser"
                target="_blank"
                rel="noopener noreferrer"
                className="btn btn-ghost btn-sm btn-square"
                aria-label="X (Twitter)"
              >
                <Twitter size={18} />
              </a>
              <a
                href="https://floorp.app/discord"
                target="_blank"
                rel="noopener noreferrer"
                className="btn btn-ghost btn-sm btn-square"
                aria-label="Discord"
              >
                <MessageCircle size={18} />
              </a>
            </div>
          </div>

          <div className="mt-8 pt-6 border-t border-neutral-content/10 text-center text-xs text-neutral-content/50">
            {t("whatsNew.footer.copyright", {
              year: new Date().getFullYear(),
            })}
          </div>
        </div>
      </footer>
    </div>
  );
}

/* ── Scroll-reveal hook ──────────────────────────────────── */
function useReveal<T extends HTMLElement>() {
  const ref = useRef<T>(null);
  useEffect(() => {
    const el = ref.current;
    if (!el) return;

    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) {
          el.classList.add("visible");
          observer.unobserve(el);
        }
      },
      { threshold: 0.15 },
    );
    observer.observe(el);
    return () => observer.disconnect();
  }, []);
  return ref;
}

function FeatureSection(props: {
  tag: string;
  title: string;
  description: string;
  image: string;
  bullets: string[];
  reverse?: boolean;
}) {
  const sectionRef = useReveal<HTMLDivElement>();

  return (
    <section className="py-10 sm:py-14 first:pt-8">
      <div
        ref={sectionRef}
        className={`reveal flex flex-col gap-8 lg:gap-14 items-center ${
          props.reverse ? "lg:flex-row-reverse" : "lg:flex-row"
        }`}
      >
        {/* Text column */}
        <div className="lg:w-1/2 space-y-5">
          <div className="inline-flex items-center rounded-full bg-primary/10 px-3 py-1 text-xs font-semibold tracking-wide text-primary">
            {props.tag}
          </div>

          <h2
            className="font-bold leading-snug text-base-content"
            style={{ fontSize: "clamp(1.5rem, 3vw, 2rem)" }}
          >
            {props.title}
          </h2>

          <p className="text-base-content/70 leading-relaxed text-[0.95rem]">
            {props.description}
          </p>

          {props.bullets.length > 0 && (
            <ul className="space-y-2.5 pt-1">
              {props.bullets.map((feature, index) => (
                <li key={index} className="flex items-start gap-3 text-sm">
                  <span className="mt-0.5 flex h-5 w-5 shrink-0 items-center justify-center rounded-md bg-primary/15 text-primary">
                    <Check size={13} strokeWidth={3} />
                  </span>
                  <span className="text-base-content/80">{feature}</span>
                </li>
              ))}
            </ul>
          )}
        </div>

        {/* Image column */}
        <div className="lg:w-1/2 flex justify-center">
          <div className="relative w-full max-w-md rounded-2xl bg-base-200 p-6 sm:p-8 ring-1 ring-base-300">
            <img
              src={props.image}
              alt={props.title}
              className="w-full h-auto object-contain max-h-72"
            />
          </div>
        </div>
      </div>
    </section>
  );
}
