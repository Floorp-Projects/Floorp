/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useMemo, useState } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import type { ComponentType, SVGProps } from "react";

type LucideIconComponent = ComponentType<
  SVGProps<SVGSVGElement> & { size?: number | string }
>;

interface BuiltinOption {
  label: string;
  value: string;
  icon?: string;
}

interface IconPickerFieldProps {
  options: BuiltinOption[];
  value: string;
  placeholder?: string;
  onChange: (value: string) => void;
}

const MAX_LIBRARY_RESULTS = 60;

const pascalToWords = (name: string): string =>
  name.replace(/([a-z0-9])([A-Z])/g, "$1 $2").toLowerCase();

/* Material Symbols (outlined, 400): plain SVG files globbed lazily from
 * the package. Names are available instantly for search; a file is only
 * fetched when its cell actually renders. */
const materialModules = import.meta.glob(
  "../../../../../node_modules/@material-symbols/svg-400/outlined/*.svg",
  { query: "?raw", import: "default" },
) as Record<string, () => Promise<string>>;

const materialLoaderByName = new Map(
  Object.entries(materialModules).map(([path, load]) => [
    path.split("/").pop()!.replace(/\.svg$/, ""),
    load,
  ]),
);

const materialNames = [...materialLoaderByName.keys()].filter(
  (n) => !n.endsWith("-fill"),
);

const materialDataUriCache = new Map<string, string>();

/** Same storage contract as lucideToDataUri: context-fill so chrome CSS
 * tints the stored icon via -moz-context-properties. */
const materialToDataUri = (raw: string, name: string): string => {
  const themed = raw
    .replace(/(<svg[^>]*?)\sfill="[^"]*"/, "$1")
    .replace(/<svg /, `<svg fill="context-fill" data-floorp-icon-name="material:${name}" `);
  return `data:image/svg+xml;utf8,${encodeURIComponent(themed)}`;
};

function MaterialCell(props: {
  name: string;
  selected: boolean;
  cellClass: string;
  onPick: (uri: string) => void;
}) {
  const { name, cellClass, onPick } = props;
  const [uri, setUri] = useState<string | null>(
    materialDataUriCache.get(name) ?? null,
  );

  useEffect(() => {
    if (uri) return;
    let live = true;
    materialLoaderByName.get(name)?.().then((raw) => {
      const converted = materialToDataUri(raw, name);
      materialDataUriCache.set(name, converted);
      if (live) setUri(converted);
    });
    return () => {
      live = false;
    };
  }, [name, uri]);

  return (
    <button
      type="button"
      title={name.replaceAll("_", " ")}
      className={cellClass}
      onClick={() => {
        if (uri) onPick(uri);
      }}
    >
      {/* context-fill renders black in a plain img; dark:invert keeps the
          preview visible on the dark theme. */}
      {uri
        ? <img src={uri} className="w-4 h-4 dark:invert" alt="" />
        : <span className="w-4 h-4" />}
    </button>
  );
}

/**
 * Render a Lucide icon to a standalone SVG data URI so the stored workspace
 * icon never needs the icon library at display time. currentColor is swapped
 * for context-stroke/fill, which Firefox chrome CSS can tint via
 * -moz-context-properties.
 */
const lucideToDataUri = (Icon: LucideIconComponent, name: string): string => {
  const svg = renderToStaticMarkup(
    <Icon xmlns="http://www.w3.org/2000/svg" size={16} strokeWidth={2} />,
  );
  const themed = svg
    .replaceAll('stroke="currentColor"', 'stroke="context-stroke"')
    .replaceAll('fill="currentColor"', 'fill="context-fill"')
    // The picked name rides inside the stored SVG so the picker can show
    // which of the thousands of icons this is when the form reopens.
    .replace(/<svg /, `<svg data-floorp-icon-name="${name}" `);
  try {
    return `data:image/svg+xml;base64,${btoa(themed)}`;
  } catch {
    return `data:image/svg+xml;utf8,${encodeURIComponent(themed)}`;
  }
};

/** Recover the embedded icon name from a stored data-URI value. */
const iconNameFromValue = (value: string): string | null => {
  if (!value.startsWith("data:image/")) return null;
  try {
    const comma = value.indexOf(",");
    const meta = value.slice(0, comma);
    const body = value.slice(comma + 1);
    const text = meta.includes("base64") ? atob(body) : decodeURIComponent(body);
    const match = text.match(/data-floorp-icon-name="([^"]+)"/);
    return match ? match[1] : null;
  } catch {
    return null;
  }
};

const displayIconName = (raw: string): string =>
  raw.startsWith("material:")
    ? raw.slice("material:".length).replaceAll("_", " ")
    : pascalToWords(raw);

export function IconPickerField(
  { options, value, placeholder, onChange }: IconPickerFieldProps,
) {
  const [query, setQuery] = useState("");
  const [lucideIcons, setLucideIcons] = useState<
    Record<string, LucideIconComponent> | null
  >(null);
  // Seeded from the stored value so reopening the form shows which icon
  // is currently selected (and highlights its cell when it scrolls by).
  const [pickedLibraryName, setPickedLibraryName] = useState<string | null>(
    () => iconNameFromValue(value),
  );

  useEffect(() => {
    let mounted = true;
    // Lazy: the full icon set only loads when the picker is on screen.
    import("lucide-react").then((mod) => {
      if (mounted) {
        setLucideIcons(mod.icons as Record<string, LucideIconComponent>);
      }
    });
    return () => {
      mounted = false;
    };
  }, []);

  const normalizedQuery = query.trim().toLowerCase();

  const filteredBuiltins = useMemo(() => {
    if (!normalizedQuery) return options;
    return options.filter(
      (opt) =>
        opt.label.toLowerCase().includes(normalizedQuery) ||
        opt.value.toLowerCase().includes(normalizedQuery),
    );
  }, [options, normalizedQuery]);

  const filteredLucide = useMemo(() => {
    if (!lucideIcons) return [];
    const names = Object.keys(lucideIcons);
    const matches = normalizedQuery
      ? names.filter((name) => pascalToWords(name).includes(normalizedQuery))
      : names;
    return matches.slice(0, MAX_LIBRARY_RESULTS);
  }, [lucideIcons, normalizedQuery]);

  const filteredMaterial = useMemo(() => {
    const matches = normalizedQuery
      ? materialNames.filter((name) =>
        name.replaceAll("_", " ").includes(normalizedQuery)
      )
      : materialNames;
    return matches.slice(0, MAX_LIBRARY_RESULTS);
  }, [normalizedQuery]);

  const isCustomValue = value.startsWith("data:image/");

  const cellClass = (selected: boolean) =>
    `flex items-center justify-center h-8 w-8 rounded-md cursor-pointer border transition duration-100 ${
      selected
        ? "border-[#0061E0] ring-2 ring-[#0061E0] bg-blue-50 dark:bg-[#003b8e]"
        : "border-transparent hover:bg-gray-100 dark:hover:bg-gray-600"
    }`;

  return (
    <div>
      <input
        type="text"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
        placeholder={placeholder || "Search icons…"}
        className="w-full px-4 py-2 mb-2 text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out"
      />
      <div className="grid grid-cols-10 gap-1 max-h-52 overflow-y-auto scrollbar-thin p-1 border border-gray-300 dark:border-[#42414D] rounded-md">
        {filteredBuiltins.map((opt) => (
          <button
            key={opt.value}
            type="button"
            title={opt.label}
            className={cellClass(!isCustomValue && value === opt.value)}
            onClick={() => {
              setPickedLibraryName(null);
              onChange(opt.value);
            }}
          >
            {opt.icon ? <img src={opt.icon} className="w-4 h-4" alt="" /> : (
              <span className="text-xs">{opt.label.slice(0, 2)}</span>
            )}
          </button>
        ))}
        {filteredLucide.map((name) => {
          const Icon = lucideIcons?.[name];
          if (!Icon) return null;
          return (
            <button
              key={`lucide-${name}`}
              type="button"
              title={pascalToWords(name)}
              className={cellClass(
                isCustomValue && pickedLibraryName === name,
              )}
              onClick={() => {
                setPickedLibraryName(name);
                onChange(lucideToDataUri(Icon, name));
              }}
            >
              <Icon size={16} className="text-gray-900 dark:text-white" />
            </button>
          );
        })}
        {filteredMaterial.map((name) => (
          <MaterialCell
            key={`material-${name}`}
            name={name}
            selected={isCustomValue && pickedLibraryName === `material:${name}`}
            cellClass={cellClass(
              isCustomValue && pickedLibraryName === `material:${name}`,
            )}
            onPick={(uri) => {
              setPickedLibraryName(`material:${name}`);
              onChange(uri);
            }}
          />
        ))}
      </div>
      <div className="mt-1 text-xs text-gray-500 dark:text-gray-400">
        {!lucideIcons
          ? "Loading icon library…"
          : isCustomValue && pickedLibraryName
          ? (
            <span className="inline-flex items-center gap-1">
              Selected:
              <img src={value} className="w-4 h-4 inline dark:invert" alt="" />
              <span className="font-medium">
                {displayIconName(pickedLibraryName)}
              </span>
            </span>
          )
          : isCustomValue
          ? (
            <span className="inline-flex items-center gap-1">
              Current custom icon:
              <img src={value} className="w-4 h-4 inline dark:invert" alt="" />
            </span>
          )
          : filteredLucide.length === MAX_LIBRARY_RESULTS ||
              filteredMaterial.length === MAX_LIBRARY_RESULTS
          ? "Showing first matches — type to refine (Lucide + Material)"
          : `${
            filteredBuiltins.length + filteredLucide.length +
            filteredMaterial.length
          } icons`}
      </div>
    </div>
  );
}
