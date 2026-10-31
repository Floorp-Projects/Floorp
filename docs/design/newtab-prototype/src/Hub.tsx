import React, { useRef, useState, type ReactNode } from "react";
import {
  ArrowLeft,
  ChevronDown,
  ExternalLink,
  Grid2X2,
  Home,
  Info,
  Keyboard,
  Menu,
  MousePointer2,
  PanelLeft,
  PanelsTopLeft,
  Search,
  SlidersHorizontal,
  X,
} from "lucide-react";
import { Button as HubButton } from "../../../../libs/ui/button.tsx";
import { Input } from "../../../../libs/ui/input.tsx";
import { Switch } from "../../../../libs/ui/switch.tsx";
import { FloorpBrand } from "../../../../libs/ui/brand.tsx";
import { groups, workspaces } from "./data.ts";
import { usePrototype } from "./store.tsx";
import type { GroupId, HubTab, Settings } from "./types.ts";
import { IconButton } from "./ui.tsx";
import { BackgroundSettings } from "./BackgroundSettings.tsx";
import "./hub.css";

const tabs: { id: HubTab; title: string }[] = [
  { id: "general", title: "表示とレイアウト" },
  { id: "dial", title: "スピードダイアル" },
  { id: "feed", title: "フィード" },
  { id: "sponsors", title: "スポンサー" },
];

function HubSidebar({ close }: { close?: () => void }) {
  return (
    <div className="hub-sidebar-content">
      <div className="hub-sidebar-brand">
        <FloorpBrand onDark />
        {close && (
          <IconButton
            label="ナビゲーションを閉じる"
            onClick={close}
            className="hub-nav-close"
          >
            <X size={20} />
          </IconButton>
        )}
      </div>
      <nav className="hub-navigation" aria-label="Floorp Hub">
        <span className="hub-nav-item hub-nav-static">
          <Home size={19} />
          ホーム
        </span>
        <p className="hub-nav-group">ブラウザーをカスタマイズ</p>
        <span className="hub-nav-item hub-nav-static">
          <SlidersHorizontal size={19} />
          外観とタブ
        </span>
        <span className="hub-nav-item hub-nav-selected" aria-current="page">
          <PanelsTopLeft size={19} />
          新しいタブ
        </span>
        <span className="hub-nav-item hub-nav-static">
          <Grid2X2 size={19} />
          ワークスペース
        </span>
        <span className="hub-nav-item hub-nav-static">
          <PanelLeft size={19} />
          サイドバー
        </span>
        <span className="hub-nav-item hub-nav-static">
          <Keyboard size={19} />
          キーボードショートカット
        </span>
        <span className="hub-nav-item hub-nav-static">
          <MousePointer2 size={19} />
          マウスジェスチャー
        </span>
      </nav>
      <div className="hub-sidebar-footer">
        <span className="hub-nav-item hub-nav-static">
          <Info size={19} />
          製品情報
        </span>
        <span className="hub-version">Floorp 12.19.0</span>
      </div>
    </div>
  );
}

function SettingRow({
  title,
  description,
  children,
  htmlFor,
}: {
  title: string;
  description?: string;
  children: ReactNode;
  htmlFor?: string;
}) {
  const label = (
    <>
      <span className="hub-setting-label">{title}</span>
      {description && <span className="hub-setting-hint">{description}</span>}
    </>
  );
  return (
    <div className="hub-setting-row">
      {htmlFor ? <label htmlFor={htmlFor}>{label}</label> : <div>{label}</div>}
      <div className="hub-setting-control">{children}</div>
    </div>
  );
}

function LayoutPreview({ layout }: { layout: Settings["layout"] }) {
  return (
    <span
      className={`hub-layout-preview hub-layout-preview-${layout}`}
      aria-hidden="true"
    >
      <span className="hub-preview-search" />
      <span className="hub-preview-grid">
        <i />
        <i />
        <i />
        <i />
        <i />
        <i />
      </span>
      <span className="hub-preview-feed">
        <i />
        <i />
        <i />
      </span>
    </span>
  );
}

function HubSelect({
  id,
  value,
  onChange,
  children,
}: {
  id: string;
  value: string | number;
  onChange: React.ChangeEventHandler<HTMLSelectElement>;
  children: ReactNode;
}) {
  return (
    <span className="hub-select-wrap">
      <select id={id} className="hub-select" value={value} onChange={onChange}>
        {children}
      </select>
      <ChevronDown size={16} aria-hidden="true" />
    </span>
  );
}

export function Hub() {
  const { data, setData, hubTab, setHubTab, navigate, showToast } =
    usePrototype();
  const [query, setQuery] = useState("");
  const mobileNav = useRef<HTMLDialogElement>(null);
  const updateSetting = <K extends keyof Settings>(
    key: K,
    value: Settings[K],
  ) => {
    setData((current) => ({
      ...current,
      settings: { ...current.settings, [key]: value },
    }));
  };
  const sections: {
    id: string;
    tab: HubTab;
    title: string;
    description: string;
    keywords: string;
    content: ReactNode;
  }[] = [
    {
      id: "visibility",
      tab: "general",
      title: "表示する項目",
      description: "新しいタブに表示するものを選べます。",
      keywords: "検索バー スピードダイアル フィード 時計 表示 非表示",
      content: (
        <>
          {(
            [
              ["showSearch", "検索バー"],
              ["showDials", "スピードダイアル"],
              ["showFeed", "フィード"],
              ["showClock", "時計"],
            ] as const
          ).map(([key, title]) => (
            <SettingRow key={key} title={title} htmlFor={`hub-${key}`}>
              <Switch
                id={`hub-${key}`}
                checked={data.settings[key]}
                onChange={(event) =>
                  updateSetting(key, event.currentTarget.checked)
                }
              />
            </SettingRow>
          ))}
        </>
      ),
    },
    {
      id: "layout",
      tab: "general",
      title: "ホームのレイアウト",
      description: "よく使うものを、使いやすい位置に。",
      keywords: "バランス リンク中心 読む中心 配置 レイアウト ホーム",
      content: (
        <fieldset className="hub-layout-options">
          <legend className="hub-sr-only">ホームのレイアウト</legend>
          {(
            [
              ["balanced", "バランス"],
              ["links", "リンク中心"],
              ["reading", "読む中心"],
            ] as const
          ).map(([value, title]) => (
            <label
              key={value}
              className="hub-layout-option"
              data-selected={data.settings.layout === value}
            >
              <input
                type="radio"
                name="home-layout"
                value={value}
                checked={data.settings.layout === value}
                onChange={() => updateSetting("layout", value)}
              />
              <LayoutPreview layout={value} />
              <span>{title}</span>
            </label>
          ))}
        </fieldset>
      ),
    },
    {
      id: "background",
      tab: "general",
      title: "背景",
      description:
        "いつもの背景を、新しいタブでも。画像やスライドショーを選べます。",
      keywords:
        "背景 画像 写真 フォルダー ランダム Floorp スライドショー 秒 自分 ファイル",
      content: <BackgroundSettings />,
    },
    {
      id: "dial-display",
      tab: "dial",
      title: "タイルの表示",
      description:
        "サイトの見せ方を調整します。画面の幅に合わせて列数が変わります。",
      keywords: "タイル サイズ 列数 4列 5列 6列 標準 コンパクト",
      content: (
        <>
          <SettingRow title="列数" htmlFor="hub-columns">
            <HubSelect
              id="hub-columns"
              value={data.settings.columns}
              onChange={(event) =>
                updateSetting(
                  "columns",
                  Number(event.currentTarget.value) as Settings["columns"],
                )
              }
            >
              <option value={4}>4 列</option>
              <option value={5}>5 列</option>
              <option value={6}>6 列</option>
            </HubSelect>
          </SettingRow>
          <SettingRow title="サイズ" htmlFor="hub-tile-size">
            <HubSelect
              id="hub-tile-size"
              value={data.settings.tileSize}
              onChange={(event) =>
                updateSetting(
                  "tileSize",
                  event.currentTarget.value as Settings["tileSize"],
                )
              }
            >
              <option value="standard">標準</option>
              <option value="compact">コンパクト</option>
            </HubSelect>
          </SettingRow>
        </>
      ),
    },
    {
      id: "workspace-groups",
      tab: "dial",
      title: "ワークスペースのグループ",
      description:
        "各ワークスペースで最初に表示するブックマークのグループです。",
      keywords:
        "ワークスペース グループ 割り当て 共通 開発 デザイン 読む 個人 ブックマーク",
      content: (
        <>
          {workspaces.map((workspace) => (
            <SettingRow
              key={workspace.id}
              title={workspace.title}
              htmlFor={`hub-group-${workspace.id}`}
            >
              <HubSelect
                id={`hub-group-${workspace.id}`}
                value={data.assignments[workspace.id]}
                onChange={(event) => {
                  const groupId = event.currentTarget.value as GroupId;
                  setData((current) => ({
                    ...current,
                    assignments: {
                      ...current.assignments,
                      [workspace.id]: groupId,
                    },
                  }));
                }}
              >
                {groups.map((group) => (
                  <option key={group.id} value={group.id}>
                    {group.title}
                  </option>
                ))}
              </HubSelect>
            </SettingRow>
          ))}
          <p className="hub-section-note">
            新しいワークスペースは「共通」グループから始まります。
          </p>
        </>
      ),
    },
    {
      id: "feed-update",
      tab: "feed",
      title: "フィードの更新",
      description: "購読しているフィードの新着を確認します。",
      keywords: "RSS フィード 更新 間隔 自動 15 30 60 分",
      content: (
        <SettingRow title="自動更新の間隔" htmlFor="hub-feed-refresh">
          <HubSelect
            id="hub-feed-refresh"
            value={data.settings.refreshMinutes}
            onChange={(event) =>
              updateSetting("refreshMinutes", Number(event.currentTarget.value))
            }
          >
            <option value={15}>15 分ごと</option>
            <option value={30}>30 分ごと</option>
            <option value={60}>1 時間ごと</option>
          </HubSelect>
        </SettingRow>
      ),
    },
    {
      id: "feed-reading",
      tab: "feed",
      title: "記事を読む",
      description: "配信された記事を、専用リーダーで読みやすく。",
      keywords: "RSS 本文 リーダー 読む 未読 スター 購読 共通",
      content: (
        <div className="hub-copy-block">
          <p>
            フィードで配信された本文を表示します。抜粋の記事には、元サイトへのリンクが表示されます。
          </p>
          <p>購読先・未読・スターは、すべてのワークスペースで共通です。</p>
          <HubButton variant="secondary" onClick={() => navigate("feed")}>
            リーダーを開く
            <ExternalLink size={16} />
          </HubButton>
        </div>
      ),
    },
    {
      id: "sponsor-visibility",
      tab: "sponsors",
      title: "スポンサーの表示",
      description: "スポンサーからの収益は、Floorp の開発を支えます。",
      keywords: "スポンサー 広告 表示 非表示 ショートカット 開発 支援",
      content: (
        <>
          <SettingRow
            title="スポンサー付きショートカットを表示"
            description="Speed Dial に、2つのスポンサー枠を表示します。"
            htmlFor="hub-show-sponsors"
          >
            <Switch
              id="hub-show-sponsors"
              checked={data.settings.showSponsors}
              onChange={(event) =>
                updateSetting("showSponsors", event.currentTarget.checked)
              }
            />
          </SettingRow>
          <p className="hub-section-note">
            この設定は、すべてのワークスペースに適用されます。
          </p>
        </>
      ),
    },
    {
      id: "sponsor-restore",
      tab: "sponsors",
      title: "非表示にしたスポンサー",
      description:
        "タイルのメニューから非表示にしたスポンサーを、もう一度表示できます。",
      keywords: "スポンサー 広告 非表示 復元 元に戻す",
      content: (
        <div className="hub-restore-row">
          <p>
            {data.dismissedSponsors.length > 0
              ? `${data.dismissedSponsors.length}件のスポンサーを非表示にしています。`
              : "非表示にしたスポンサーはありません。"}
          </p>
          <HubButton
            variant="secondary"
            disabled={data.dismissedSponsors.length === 0}
            onClick={() => {
              setData((current) => ({ ...current, dismissedSponsors: [] }));
              showToast("非表示にしたスポンサーを復元しました");
            }}
          >
            表示を元に戻す
          </HubButton>
        </div>
      ),
    },
  ];
  const normalizedQuery = query.trim().toLocaleLowerCase("ja-JP");
  const shownSections = sections.filter((section) =>
    normalizedQuery
      ? `${section.title} ${section.description} ${section.keywords}`
          .toLocaleLowerCase("ja-JP")
          .includes(normalizedQuery)
      : section.tab === hubTab,
  );

  return (
    <div className="hub-screen floorp-standard-ui">
      <aside className="hub-sidebar">
        <HubSidebar />
      </aside>
      <dialog ref={mobileNav} className="hub-mobile-dialog">
        <HubSidebar close={() => mobileNav.current?.close()} />
      </dialog>
      <div className="hub-body">
        <header className="hub-topbar">
          <IconButton
            label="Floorp Hub のナビゲーションを開く"
            onClick={() => mobileNav.current?.showModal()}
            className="hub-mobile-menu"
          >
            <Menu size={21} />
          </IconButton>
          <div className="hub-search">
            <Search size={19} aria-hidden="true" />
            <Input
              type="search"
              aria-label="新しいタブの設定を検索"
              placeholder="設定を検索"
              value={query}
              onChange={(event) => setQuery(event.currentTarget.value)}
            />
          </div>
        </header>
        <main className="hub-main">
          <div className="hub-page-heading">
            <h1>新しいタブ</h1>
            <p>新しいタブを、自分の使い方に。</p>
          </div>
          <nav className="hub-tabs" aria-label="新しいタブの設定カテゴリー">
            {tabs.map((tab) => (
              <button
                key={tab.id}
                type="button"
                aria-current={
                  !normalizedQuery && hubTab === tab.id ? "page" : undefined
                }
                onClick={() => {
                  setQuery("");
                  setHubTab(tab.id);
                }}
              >
                {tab.title}
              </button>
            ))}
          </nav>
          <div className="hub-settings-sections" aria-live="polite">
            {shownSections.map((section) => (
              <section
                key={section.id}
                className="hub-setting-section"
                aria-labelledby={`hub-section-${section.id}`}
              >
                <div>
                  <h2 id={`hub-section-${section.id}`}>{section.title}</h2>
                  <p>{section.description}</p>
                </div>
                <div className="hub-section-controls">{section.content}</div>
              </section>
            ))}
            {shownSections.length === 0 && (
              <div className="hub-empty-search">
                <Search size={28} />
                <h2>一致する設定はありません</h2>
                <p>別の言葉で検索してみてください。</p>
              </div>
            )}
          </div>
          <footer className="hub-main-footer">
            <HubButton variant="ghost" onClick={() => navigate("home")}>
              <ArrowLeft size={18} />
              新しいタブに戻る
            </HubButton>
          </footer>
        </main>
      </div>
    </div>
  );
}
