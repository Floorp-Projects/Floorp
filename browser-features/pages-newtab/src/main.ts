/**
 * Main script for newtab page (TypeScript, loaded as module with defer).
 * Moved from inline script to src/main.ts per request.
 * Designed to be bundled/served by Vite (TS->ESM).
 */

const LS_LINKS_KEY = "np_links_v1";
const LS_NOTE_KEY = "np_note_v1";
const LS_BG_KEY = "np_bg_v1"; // stores array of image URLs / data URLs
const LS_BG_RANDOM_KEY = "np_bg_random_v1";

function el<T extends HTMLElement>(id: string): T {
  const e = document.getElementById(id);
  if (!e) throw new Error(`Missing element: ${id}`);
  return e as T;
}

const q = el<HTMLInputElement>("q");
const searchForm = el<HTMLFormElement>("searchForm");
const linksGrid = el<HTMLDivElement>("linksGrid");
const addLinkBtn = el<HTMLButtonElement>("addLinkBtn");
const addForm = el<HTMLFormElement>("addForm");
const linkTitle = el<HTMLInputElement>("linkTitle");
const linkUrl = el<HTMLInputElement>("linkUrl");
const saveLink = el<HTMLButtonElement>("saveLink");
const cancelAdd = el<HTMLButtonElement>("cancelAdd");
const noteEl = el<HTMLTextAreaElement>("note");
const dateEl = el<HTMLParagraphElement>("date");
const timeEl = el<HTMLDivElement>("time");
const tzEl = el<HTMLSpanElement>("tz");

 // Settings elements
const settingsBtn = el<HTMLButtonElement>("settingsBtn");
const settingsPanel = el<HTMLElement>("settingsPanel");
const bgFiles = el<HTMLInputElement>("bgFiles");
const bgUrls = el<HTMLTextAreaElement>("bgUrls");
const bgRandom = el<HTMLInputElement>("bgRandom");
const saveBg = el<HTMLButtonElement>("saveBg");
const clearBgBtn = el<HTMLButtonElement>("clearBgBtn");
const bgPreview = el<HTMLDivElement>("bgPreview");
const closeSettings = el<HTMLButtonElement>("closeSettings");

timeEl.setAttribute("role", "status");
timeEl.setAttribute("aria-live", "polite");
addLinkBtn.setAttribute("aria-controls", "addForm");
addLinkBtn.setAttribute("aria-expanded", "false");
saveLink.setAttribute("aria-label", "Save quick link");
cancelAdd.setAttribute("aria-label", "Cancel add link");

function formatDate(d: Date) {
  return d.toLocaleDateString(undefined, {
    weekday: "short",
    month: "short",
    day: "numeric",
  });
}

function updateTime() {
  const t = new Date();
  timeEl.textContent = t.toLocaleTimeString();
  dateEl.textContent = formatDate(t);
  try {
    tzEl.textContent = Intl.DateTimeFormat().resolvedOptions().timeZone;
  } catch {
    tzEl.textContent = "";
  }
}
updateTime();
setInterval(updateTime, 1000);

function isProbablyUrl(str: string | null | undefined) {
  if (!str) return false;
  if (/\s/.test(str)) return false;
  if (/^https?:\/\//i.test(str)) return true;
  return /\.[a-z]{2,}/i.test(str);
}

function openQuery(value: string | null | undefined) {
  if (!value) return;
  let v = value.trim();
  if (!v) return;
  if (isProbablyUrl(v)) {
    if (!/^https?:\/\//i.test(v)) v = "https://" + v;
    window.location.href = v;
  } else {
    const s = "https://www.google.com/search?q=" + encodeURIComponent(v);
    window.location.href = s;
  }
}

searchForm.addEventListener("submit", (e) => {
  e.preventDefault();
  openQuery(q.value);
});

// Links (stored locally)
type LinkItem = { title?: string; url: string };

function loadLinks(): LinkItem[] {
  try {
    const raw = localStorage.getItem(LS_LINKS_KEY);
    return raw ? JSON.parse(raw) : [];
  } catch {
    return [];
  }
}
function saveLinks(list: LinkItem[]) {
  try {
    localStorage.setItem(LS_LINKS_KEY, JSON.stringify(list));
  } catch {
    // ignore
  }
}
function renderLinks() {
  const list = loadLinks();
  linksGrid.innerHTML = "";
  if (list.length === 0) {
    const empty = document.createElement("div");
    empty.className = "text-sm text-gray-500";
    empty.textContent = "No links. Add frequently used sites.";
    linksGrid.appendChild(empty);
    return;
  }
  list.forEach((item, idx) => {
    const a = document.createElement("a");
    a.className =
      "card block p-3 bg-white border border-gray-200 rounded-md text-sm text-gray-700";
    a.href = item.url;
    a.textContent = item.title || item.url;
    a.target = "_self";
    a.rel = "noopener";
    a.setAttribute("role", "link");
    a.setAttribute("tabindex", "0");
    a.setAttribute("aria-label", item.title || item.url);

    a.addEventListener("keydown", (ev: KeyboardEvent) => {
      if (ev.key === "Enter") {
        window.location.href = item.url;
      } else if (ev.key === "Delete") {
        ev.preventDefault();
        if (confirm("Remove this link?")) {
          list.splice(idx, 1);
          saveLinks(list);
          renderLinks();
        }
      }
    });
    a.addEventListener("contextmenu", (ev) => {
      ev.preventDefault();
      if (confirm("Remove this link?")) {
        list.splice(idx, 1);
        saveLinks(list);
        renderLinks();
      }
    });
    linksGrid.appendChild(a);
  });
}
renderLinks();

addLinkBtn.addEventListener("click", () => {
  const isHidden = addForm.classList.toggle("hidden");
  addLinkBtn.setAttribute("aria-expanded", (!isHidden).toString());
  linkTitle.value = "";
  linkUrl.value = "";
  setTimeout(() => linkTitle.focus(), 0);
});
cancelAdd.addEventListener("click", () => {
  addForm.classList.add("hidden");
  addLinkBtn.setAttribute("aria-expanded", "false");
});
saveLink.addEventListener("click", () => {
  const title = linkTitle.value.trim();
  let url = linkUrl.value.trim();
  if (!url) {
    linkUrl.focus();
    return;
  }
  if (!/^https?:\/\//i.test(url)) url = "https://" + url;
  const list = loadLinks();
  list.push({ title, url });
  saveLinks(list);
  renderLinks();
  addForm.classList.add("hidden");
  addLinkBtn.setAttribute("aria-expanded", "false");
  addLinkBtn.focus();
});

// Note
function loadNote(): string {
  try {
    return localStorage.getItem(LS_NOTE_KEY) || "";
  } catch {
    return "";
  }
}
function saveNote(v: string) {
  try {
    localStorage.setItem(LS_NOTE_KEY, v);
  } catch {
    // ignore
  }
}
noteEl.value = loadNote();
let noteTimer: number | undefined;
noteEl.addEventListener("input", () => {
  if (noteTimer) window.clearTimeout(noteTimer);
  noteTimer = window.setTimeout(() => saveNote(noteEl.value.trim()), 500);
});

/* Background images (settings) */
type BgItem = { src: string };

function loadBackgrounds(): string[] {
  try {
    const raw = localStorage.getItem(LS_BG_KEY);
    return raw ? JSON.parse(raw) : [];
  } catch {
    return [];
  }
}
function saveBackgrounds(list: string[]) {
  try {
    localStorage.setItem(LS_BG_KEY, JSON.stringify(list));
  } catch {
    // ignore
  }
}
function loadRandomFlag(): boolean {
  try {
    return localStorage.getItem(LS_BG_RANDOM_KEY) === "1";
  } catch {
    return false;
  }
}
function saveRandomFlag(v: boolean) {
  try {
    localStorage.setItem(LS_BG_RANDOM_KEY, v ? "1" : "0");
  } catch {
    // ignore
  }
}

function applyBackgroundFromList(list: string[]) {
  if (!list || list.length === 0) {
    document.body.style.backgroundImage = "";
    document.body.style.backgroundColor = "";
    document.body.style.backgroundSize = "";
    document.body.classList.remove("has-bg");
    return;
  }
  const random = loadRandomFlag();
  const idx = random ? Math.floor(Math.random() * list.length) : 0;
  const url = list[idx];
  // apply as cover
  document.body.style.backgroundImage = `url("${url}")`;
  document.body.style.backgroundSize = "cover";
  document.body.style.backgroundPosition = "center";
  document.body.style.backgroundRepeat = "no-repeat";
  document.body.classList.add("has-bg");
}

function renderBgPreviewList(list: string[]) {
  if (!bgPreview) return;
  bgPreview.innerHTML = "";
  list.forEach((src, idx) => {
    const wrap = document.createElement("div");
    wrap.className = "p-1";
    const img = document.createElement("img");
    img.src = src;
    img.alt = `background ${idx + 1}`;
    img.style.width = "100%";
    img.style.height = "64px";
    img.style.objectFit = "cover";
    img.tabIndex = 0;
    img.style.cursor = "pointer";
    img.addEventListener("click", () => {
      // set selected as only background (non-random)
      saveRandomFlag(false);
      saveBackgrounds([src]);
      applyBackgroundFromList([src]);
      renderBgPreviewList([src]);
    });
    img.addEventListener("keydown", (e) => {
      if (e.key === "Enter") {
        img.click();
      } else if (e.key === "Delete") {
        const newList = list.slice();
        newList.splice(idx, 1);
        saveBackgrounds(newList);
        renderBgPreviewList(newList);
        applyBackgroundFromList(newList);
      }
    });
    wrap.appendChild(img);
    // add small remove control
    const rm = document.createElement("button");
    rm.type = "button";
    rm.className = "text-sm";
    rm.textContent = "Remove";
    rm.style.display = "block";
    rm.addEventListener("click", () => {
      const newList = list.slice();
      newList.splice(idx, 1);
      saveBackgrounds(newList);
      renderBgPreviewList(newList);
      applyBackgroundFromList(newList);
    });
    wrap.appendChild(rm);
    bgPreview.appendChild(wrap);
  });
}

/* Settings: open/close, file upload, URLs, save, clear */
if (settingsBtn && settingsPanel && bgFiles && bgUrls && bgRandom && saveBg && clearBgBtn && bgPreview && closeSettings) {
  function showSettings(show: boolean) {
    if (show) {
      settingsPanel.classList.remove("hidden");
      settingsPanel.setAttribute("aria-hidden", "false");
      settingsBtn.setAttribute("aria-expanded", "true");
    } else {
      settingsPanel.classList.add("hidden");
      settingsPanel.setAttribute("aria-hidden", "true");
      settingsBtn.setAttribute("aria-expanded", "false");
    }
  }

  settingsBtn.addEventListener("click", () => {
    const isHidden = settingsPanel.classList.toggle("hidden");
    showSettings(!isHidden);
    // populate current state in UI
    const list = loadBackgrounds();
    renderBgPreviewList(list);
    bgUrls.value = (list || []).filter(Boolean).join("\n");
    bgRandom.checked = loadRandomFlag();
  });

  closeSettings.addEventListener("click", () => showSettings(false));
  clearBgBtn.addEventListener("click", () => {
    saveBackgrounds([]);
    saveRandomFlag(false);
    renderBgPreviewList([]);
    applyBackgroundFromList([]);
    bgUrls.value = "";
    bgRandom.checked = false;
  });

  // handle file uploads -> convert to data URLs
  bgFiles.addEventListener("change", () => {
    const files = Array.from(bgFiles.files || []);
    if (files.length === 0) return;
    const readers = files.map(
      (f) =>
        new Promise<string | null>((res) => {
          const r = new FileReader();
          r.onload = () => res(typeof r.result === "string" ? r.result : null);
          r.onerror = () => res(null);
          r.readAsDataURL(f);
        })
    );
    Promise.all(readers).then((results) => {
      const images = results.filter(Boolean) as string[];
      const existing = loadBackgrounds();
      const merged = existing.concat(images);
      saveBackgrounds(merged);
      renderBgPreviewList(merged);
      // keep URLs textarea in sync
      bgUrls.value = merged.join("\n");
      applyBackgroundFromList(merged);
      // clear file input
      bgFiles.value = "";
    });
  });

  saveBg.addEventListener("click", () => {
    const urls = (bgUrls.value || "")
      .split("\n")
      .map((s) => s.trim())
      .filter(Boolean);
    const existing = loadBackgrounds();
    // combine: urls plus existing data URLs (keep data URLs and urls)
    const dataUrls = existing.filter((s) => s.startsWith("data:"));
    const final = dataUrls.concat(urls);
    saveBackgrounds(final);
    saveRandomFlag(!!bgRandom.checked);
    renderBgPreviewList(final);
    applyBackgroundFromList(final);
    showSettings(false);
  });
}

/* Apply backgrounds on load */
applyBackgroundFromList(loadBackgrounds());

// Keyboard: Ctrl/Cmd+L focus search; Esc closes add form
document.addEventListener("keydown", (e) => {
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "l") {
    e.preventDefault();
    q.focus();
    q.select();
    return;
  }
  if (e.key === "Escape") {
    if (!addForm.classList.contains("hidden")) {
      addForm.classList.add("hidden");
      addLinkBtn.setAttribute("aria-expanded", "false");
      addLinkBtn.focus();
      return;
    }
    // also close settings if open
    if (settingsPanel && !settingsPanel.classList.contains("hidden")) {
      settingsPanel.classList.add("hidden");
      settingsBtn?.setAttribute("aria-expanded", "false");
      settingsBtn?.focus();
    }
  }
});
