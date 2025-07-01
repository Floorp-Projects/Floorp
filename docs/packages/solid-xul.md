# Solid-XUL - SolidJS と XUL の統合

## 概要

Solid-XUL (`src/packages/solid-xul/`) は、SolidJS の現代的なリアクティブ UI フレームワークと Firefox の XUL (XML User Interface Language) システムを統合するためのブリッジライブラリです。これにより、開発者は SolidJS の強力な機能を使用して Firefox の UI を構築できます。

## アーキテクチャ

### 統合レイヤー

```
┌─────────────────────────────────────────────────────────────┐
│                    SolidJS Application                      │
├─────────────────────────────────────────────────────────────┤
│                    Solid-XUL Bridge                         │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ JSX Runtime │ │ Component   │ │   Event Handling    │   │
│  │   Layer     │ │   Mapping   │ │      System         │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    Firefox XUL System                       │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ XUL Elements│ │ XPCOM APIs  │ │   Browser Services  │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## 主要ファイル

### 1. index.ts - メイン API

```typescript
// Solid-XUL のメイン API エクスポート
export * from "./jsx-runtime";
export * from "./components";
export * from "./hooks";
export * from "./utils";

// XUL 要素の型定義
export interface XULElement extends Element {
  setAttribute(name: string, value: string): void;
  getAttribute(name: string): string | null;
  removeAttribute(name: string): void;
}

// XUL 名前空間の定義
export const XUL_NAMESPACE = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// XUL 要素の作成ヘルパー
export function createXULElement(tagName: string): XULElement {
  return document.createElementNS(XUL_NAMESPACE, tagName) as XULElement;
}

// SolidJS コンポーネントを XUL に変換
export function solidToXUL(component: any, container: Element): void {
  const dispose = render(component, container);
  
  // クリーンアップ関数を返す
  return dispose;
}
```

### 2. jsx-runtime.ts - JSX ランタイム

```typescript
// SolidJS JSX を XUL 要素にマッピング
import { createRenderer } from "solid-js/universal";

// XUL 要素マッピング
const XUL_ELEMENTS = {
  // ウィンドウとダイアログ
  window: "window",
  dialog: "dialog",
  dialogheader: "dialogheader",
  
  // レイアウト
  vbox: "vbox",
  hbox: "hbox",
  box: "box",
  grid: "grid",
  rows: "rows",
  row: "row",
  columns: "columns",
  column: "column",
  
  // UI コントロール
  button: "button",
  checkbox: "checkbox",
  radio: "radio",
  textbox: "textbox",
  listbox: "listbox",
  listitem: "listitem",
  menulist: "menulist",
  menuitem: "menuitem",
  
  // メニューとツールバー
  menubar: "menubar",
  menu: "menu",
  menupopup: "menupopup",
  toolbar: "toolbar",
  toolbarbutton: "toolbarbutton",
  
  // その他
  label: "label",
  description: "description",
  separator: "separator",
  spacer: "spacer",
  progressmeter: "progressmeter"
};

// カスタム JSX ランタイム
export const {
  render,
  effect,
  memo,
  createComponent,
  createElement,
  createTextNode,
  insertNode,
  insert,
  spread,
  setProp,
  mergeProps
} = createRenderer({
  createElement(tagName: string): Element {
    // XUL 要素の場合は XUL 名前空間で作成
    if (XUL_ELEMENTS[tagName as keyof typeof XUL_ELEMENTS]) {
      return document.createElementNS(XUL_NAMESPACE, tagName);
    }
    
    // 通常の HTML 要素
    return document.createElement(tagName);
  },
  
  createTextNode(value: string): Text {
    return document.createTextNode(value);
  },
  
  replaceText(textNode: Text, value: string): void {
    textNode.data = value;
  },
  
  insertNode(parent: Element, node: Node, anchor?: Node): void {
    if (anchor) {
      parent.insertBefore(node, anchor);
    } else {
      parent.appendChild(node);
    }
  },
  
  removeNode(parent: Element, node: Node): void {
    parent.removeChild(node);
  },
  
  setProperty(node: Element, name: string, value: any): void {
    if (name.startsWith("on") && typeof value === "function") {
      // イベントリスナーの設定
      const eventName = name.slice(2).toLowerCase();
      node.addEventListener(eventName, value);
    } else if (name === "class" || name === "className") {
      // CSS クラスの設定
      node.setAttribute("class", value);
    } else if (name === "style" && typeof value === "object") {
      // スタイルオブジェクトの設定
      Object.assign((node as HTMLElement).style, value);
    } else {
      // 通常の属性設定
      if (value === null || value === undefined || value === false) {
        node.removeAttribute(name);
      } else {
        node.setAttribute(name, String(value));
      }
    }
  },
  
  getProperty(node: Element, name: string): any {
    return node.getAttribute(name);
  },
  
  getFirstChild(node: Element): Node | null {
    return node.firstChild;
  },
  
  getNextSibling(node: Node): Node | null {
    return node.nextSibling;
  }
});

// JSX 型定義
declare global {
  namespace JSX {
    interface IntrinsicElements {
      // XUL 要素の型定義
      window: XULElementProps;
      dialog: XULElementProps;
      vbox: XULElementProps;
      hbox: XULElementProps;
      box: XULElementProps;
      button: XULButtonProps;
      checkbox: XULCheckboxProps;
      textbox: XULTextboxProps;
      menubar: XULElementProps;
      menu: XULMenuProps;
      menupopup: XULElementProps;
      menuitem: XULMenuItemProps;
      toolbar: XULElementProps;
      toolbarbutton: XULToolbarButtonProps;
      label: XULLabelProps;
      listbox: XULListboxProps;
      listitem: XULListItemProps;
    }
  }
}

// XUL 要素の共通プロパティ
interface XULElementProps {
  id?: string;
  class?: string;
  className?: string;
  style?: string | Record<string, string>;
  flex?: string | number;
  orient?: "horizontal" | "vertical";
  align?: "start" | "center" | "end" | "stretch";
  pack?: "start" | "center" | "end";
  hidden?: boolean;
  disabled?: boolean;
  onclick?: (event: Event) => void;
  oncommand?: (event: Event) => void;
  children?: any;
}

// 特定の XUL 要素のプロパティ
interface XULButtonProps extends XULElementProps {
  label?: string;
  image?: string;
  type?: "button" | "menu" | "menu-button";
  default?: boolean;
}

interface XULCheckboxProps extends XULElementProps {
  label?: string;
  checked?: boolean;
  indeterminate?: boolean;
}

interface XULTextboxProps extends XULElementProps {
  value?: string;
  placeholder?: string;
  type?: "text" | "password" | "number";
  multiline?: boolean;
  readonly?: boolean;
  oninput?: (event: Event) => void;
  onchange?: (event: Event) => void;
}

interface XULMenuProps extends XULElementProps {
  label?: string;
  accesskey?: string;
}

interface XULMenuItemProps extends XULElementProps {
  label?: string;
  accesskey?: string;
  key?: string;
  type?: "checkbox" | "radio";
  checked?: boolean;
  value?: string;
}

interface XULToolbarButtonProps extends XULElementProps {
  label?: string;
  image?: string;
  tooltiptext?: string;
  type?: "button" | "menu" | "menu-button";
}

interface XULLabelProps extends XULElementProps {
  value?: string;
  control?: string;
}

interface XULListboxProps extends XULElementProps {
  seltype?: "single" | "multiple";
  rows?: number;
  onselect?: (event: Event) => void;
}

interface XULListItemProps extends XULElementProps {
  label?: string;
  value?: string;
  selected?: boolean;
}
```

## コンポーネントライブラリ

### 1. ベースコンポーネント

```typescript
// 基本的な XUL コンポーネント
import { Component, JSX, createSignal, createEffect } from "solid-js";

// XUL Button コンポーネント
export const XULButton: Component<{
  label: string;
  onClick?: () => void;
  disabled?: boolean;
  type?: "button" | "menu" | "menu-button";
  image?: string;
  class?: string;
}> = (props) => {
  return (
    <button
      label={props.label}
      disabled={props.disabled}
      type={props.type || "button"}
      image={props.image}
      class={props.class}
      oncommand={props.onClick}
    />
  );
};

// XUL Textbox コンポーネント
export const XULTextbox: Component<{
  value?: string;
  onInput?: (value: string) => void;
  placeholder?: string;
  type?: "text" | "password" | "number";
  disabled?: boolean;
  class?: string;
}> = (props) => {
  const [internalValue, setInternalValue] = createSignal(props.value || "");
  
  createEffect(() => {
    if (props.value !== undefined) {
      setInternalValue(props.value);
    }
  });
  
  const handleInput = (event: Event) => {
    const target = event.target as HTMLInputElement;
    const newValue = target.value;
    setInternalValue(newValue);
    props.onInput?.(newValue);
  };
  
  return (
    <textbox
      value={internalValue()}
      placeholder={props.placeholder}
      type={props.type || "text"}
      disabled={props.disabled}
      class={props.class}
      oninput={handleInput}
    />
  );
};

// XUL Checkbox コンポーネント
export const XULCheckbox: Component<{
  label: string;
  checked?: boolean;
  onChange?: (checked: boolean) => void;
  disabled?: boolean;
  class?: string;
}> = (props) => {
  const [internalChecked, setInternalChecked] = createSignal(props.checked || false);
  
  createEffect(() => {
    if (props.checked !== undefined) {
      setInternalChecked(props.checked);
    }
  });
  
  const handleChange = (event: Event) => {
    const target = event.target as HTMLInputElement;
    const newChecked = target.checked;
    setInternalChecked(newChecked);
    props.onChange?.(newChecked);
  };
  
  return (
    <checkbox
      label={props.label}
      checked={internalChecked()}
      disabled={props.disabled}
      class={props.class}
      oncommand={handleChange}
    />
  );
};

// XUL Menu コンポーネント
export const XULMenu: Component<{
  label: string;
  children: JSX.Element;
  accesskey?: string;
  class?: string;
}> = (props) => {
  return (
    <menu
      label={props.label}
      accesskey={props.accesskey}
      class={props.class}
    >
      <menupopup>
        {props.children}
      </menupopup>
    </menu>
  );
};

// XUL MenuItem コンポーネント
export const XULMenuItem: Component<{
  label: string;
  onClick?: () => void;
  accesskey?: string;
  key?: string;
  type?: "checkbox" | "radio";
  checked?: boolean;
  disabled?: boolean;
  class?: string;
}> = (props) => {
  return (
    <menuitem
      label={props.label}
      accesskey={props.accesskey}
      key={props.key}
      type={props.type}
      checked={props.checked}
      disabled={props.disabled}
      class={props.class}
      oncommand={props.onClick}
    />
  );
};
```

### 2. レイアウトコンポーネント

```typescript
// レイアウト用コンポーネント
export const VBox: Component<{
  children: JSX.Element;
  flex?: number;
  align?: "start" | "center" | "end" | "stretch";
  pack?: "start" | "center" | "end";
  class?: string;
}> = (props) => {
  return (
    <vbox
      flex={props.flex}
      align={props.align}
      pack={props.pack}
      class={props.class}
    >
      {props.children}
    </vbox>
  );
};

export const HBox: Component<{
  children: JSX.Element;
  flex?: number;
  align?: "start" | "center" | "end" | "stretch";
  pack?: "start" | "center" | "end";
  class?: string;
}> = (props) => {
  return (
    <hbox
      flex={props.flex}
      align={props.align}
      pack={props.pack}
      class={props.class}
    >
      {props.children}
    </hbox>
  );
};

export const Toolbar: Component<{
  children: JSX.Element;
  id?: string;
  class?: string;
}> = (props) => {
  return (
    <toolbar
      id={props.id}
      class={props.class}
    >
      {props.children}
    </toolbar>
  );
};
```

## フックシステム

### 1. XUL 要素フック

```typescript
// XUL 要素を扱うためのフック
import { createSignal, onMount, onCleanup } from "solid-js";

// XUL 要素の参照を取得
export function useXULRef<T extends XULElement = XULElement>() {
  const [element, setElement] = createSignal<T | null>(null);
  
  const ref = (el: T) => {
    setElement(el);
  };
  
  return [element, ref] as const;
}

// XUL 属性の監視
export function useXULAttribute(element: () => XULElement | null, attributeName: string) {
  const [value, setValue] = createSignal<string | null>(null);
  
  createEffect(() => {
    const el = element();
    if (el) {
      setValue(el.getAttribute(attributeName));
      
      const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
          if (mutation.type === 'attributes' && mutation.attributeName === attributeName) {
            setValue(el.getAttribute(attributeName));
          }
        });
      });
      
      observer.observe(el, { attributes: true, attributeFilter: [attributeName] });
      
      onCleanup(() => {
        observer.disconnect();
      });
    }
  });
  
  return value;
}

// XUL イベントリスナー
export function useXULEvent<T extends Event = Event>(
  element: () => XULElement | null,
  eventName: string,
  handler: (event: T) => void,
  options?: AddEventListenerOptions
) {
  createEffect(() => {
    const el = element();
    if (el) {
      el.addEventListener(eventName, handler as EventListener, options);
      
      onCleanup(() => {
        el.removeEventListener(eventName, handler as EventListener, options);
      });
    }
  });
}
```

### 2. Firefox API フック

```typescript
// Firefox の API を SolidJS で使いやすくするフック
export function useFirefoxPreference(prefName: string, defaultValue?: any) {
  const [value, setValue] = createSignal(defaultValue);
  
  onMount(() => {
    // 設定値の初期読み込み
    try {
      const Services = (window as any).Services;
      const prefValue = Services.prefs.getStringPref(prefName, defaultValue);
      setValue(prefValue);
      
      // 設定変更の監視
      const observer = {
        observe: (subject: any, topic: string, data: string) => {
          if (data === prefName) {
            const newValue = Services.prefs.getStringPref(prefName, defaultValue);
            setValue(newValue);
          }
        }
      };
      
      Services.prefs.addObserver(prefName, observer, false);
      
      onCleanup(() => {
        Services.prefs.removeObserver(prefName, observer);
      });
    } catch (error) {
      console.error('Failed to access Firefox preferences:', error);
    }
  });
  
  const updatePreference = (newValue: any) => {
    try {
      const Services = (window as any).Services;
      Services.prefs.setStringPref(prefName, String(newValue));
      setValue(newValue);
    } catch (error) {
      console.error('Failed to update Firefox preference:', error);
    }
  };
  
  return [value, updatePreference] as const;
}

// タブ情報の監視
export function useTabInfo() {
  const [currentTab, setCurrentTab] = createSignal(null);
  const [allTabs, setAllTabs] = createSignal([]);
  
  onMount(() => {
    const gBrowser = (window as any).gBrowser;
    if (gBrowser) {
      // 現在のタブ情報を設定
      setCurrentTab(gBrowser.selectedTab);
      setAllTabs(Array.from(gBrowser.tabs));
      
      // タブ変更イベントの監視
      const handleTabSelect = (event: Event) => {
        setCurrentTab(gBrowser.selectedTab);
      };
      
      const handleTabOpen = (event: Event) => {
        setAllTabs(Array.from(gBrowser.tabs));
      };
      
      const handleTabClose = (event: Event) => {
        setAllTabs(Array.from(gBrowser.tabs));
      };
      
      gBrowser.addEventListener('TabSelect', handleTabSelect);
      gBrowser.addEventListener('TabOpen', handleTabOpen);
      gBrowser.addEventListener('TabClose', handleTabClose);
      
      onCleanup(() => {
        gBrowser.removeEventListener('TabSelect', handleTabSelect);
        gBrowser.removeEventListener('TabOpen', handleTabOpen);
        gBrowser.removeEventListener('TabClose', handleTabClose);
      });
    }
  });
  
  return { currentTab, allTabs };
}
```

## ユーティリティ関数

### 1. XUL DOM 操作

```typescript
// XUL DOM 操作のユーティリティ
export class XULUtils {
  // XUL 要素の検索
  static findElement(selector: string): XULElement | null {
    return document.querySelector(selector) as XULElement;
  }
  
  static findElements(selector: string): XULElement[] {
    return Array.from(document.querySelectorAll(selector)) as XULElement[];
  }
  
  // XUL 要素の作成
  static createElement(tagName: string, attributes: Record<string, string> = {}): XULElement {
    const element = document.createElementNS(XUL_NAMESPACE, tagName) as XULElement;
    
    Object.entries(attributes).forEach(([name, value]) => {
      element.setAttribute(name, value);
    });
    
    return element;
  }
  
  // メニューアイテムの動的追加
  static addMenuItem(
    menuPopup: XULElement,
    label: string,
    command: () => void,
    options: {
      accesskey?: string;
      key?: string;
      type?: "checkbox" | "radio";
      checked?: boolean;
    } = {}
  ): XULElement {
    const menuItem = this.createElement('menuitem', {
      label,
      ...(options.accesskey && { accesskey: options.accesskey }),
      ...(options.key && { key: options.key }),
      ...(options.type && { type: options.type }),
      ...(options.checked && { checked: String(options.checked) })
    });
    
    menuItem.addEventListener('command', command);
    menuPopup.appendChild(menuItem);
    
    return menuItem;
  }
  
  // ツールバーボタンの動的追加
  static addToolbarButton(
    toolbar: XULElement,
    id: string,
    label: string,
    command: () => void,
    options: {
      image?: string;
      tooltiptext?: string;
      type?: "button" | "menu" | "menu-button";
    } = {}
  ): XULElement {
    const button = this.createElement('toolbarbutton', {
      id,
      label,
      ...(options.image && { image: options.image }),
      ...(options.tooltiptext && { tooltiptext: options.tooltiptext }),
      ...(options.type && { type: options.type })
    });
    
    button.addEventListener('command', command);
    toolbar.appendChild(button);
    
    return button;
  }
}
```

### 2. イベント処理

```typescript
// XUL イベント処理のユーティリティ
export class XULEventUtils {
  // カスタムイベントの作成と発火
  static dispatchCustomEvent(
    element: XULElement,
    eventName: string,
    detail?: any
  ): boolean {
    const event = new CustomEvent(eventName, {
      detail,
      bubbles: true,
      cancelable: true
    });
    
    return element.dispatchEvent(event);
  }
  
  // キーボードショートカットの登録
  static registerKeyShortcut(
    key: string,
    modifiers: string[],
    handler: () => void
  ): () => void {
    const keyElement = XULUtils.createElement('key', {
      key,
      modifiers: modifiers.join(','),
      oncommand: 'void(0);' // XUL では oncommand 属性が必要
    });
    
    keyElement.addEventListener('command', handler);
    
    // keyset に追加
    let keyset = document.getElementById('mainKeyset');
    if (!keyset) {
      keyset = XULUtils.createElement('keyset', { id: 'mainKeyset' });
      document.documentElement.appendChild(keyset);
    }
    
    keyset.appendChild(keyElement);
    
    // クリーンアップ関数を返す
    return () => {
      if (keyElement.parentNode) {
        keyElement.parentNode.removeChild(keyElement);
      }
    };
  }
}
```

## 使用例

### 1. 基本的な使用法

```typescript
// SolidJS + XUL の基本的な使用例
import { render } from "solid-js/web";
import { createSignal } from "solid-js";
import { XULButton, XULTextbox, VBox, HBox } from "@nora/solid-xul";

function MyXULApp() {
  const [text, setText] = createSignal("");
  const [count, setCount] = createSignal(0);
  
  return (
    <VBox class="main-container" flex={1}>
      <HBox align="center" class="input-row">
        <label value="テキスト入力:" />
        <XULTextbox
          value={text()}
          onInput={setText}
          placeholder="何か入力してください"
          flex={1}
        />
      </HBox>
      
      <HBox align="center" class="button-row">
        <XULButton
          label={`クリック数: ${count()}`}
          onClick={() => setCount(c => c + 1)}
        />
        <XULButton
          label="リセット"
          onClick={() => {
            setCount(0);
            setText("");
          }}
        />
      </HBox>
      
      <VBox flex={1} class="output-area">
        <label value="入力されたテキスト:" />
        <description>{text()}</description>
      </VBox>
    </VBox>
  );
}

// XUL ウィンドウに描画
const container = document.getElementById("main-content");
if (container) {
  render(() => <MyXULApp />, container);
}
```

### 2. Firefox API との統合

```typescript
// Firefox API を使用した高度な例
import { useFirefoxPreference, useTabInfo } from "@nora/solid-xul";

function BrowserIntegrationExample() {
  const [homePage, setHomePage] = useFirefoxPreference("browser.startup.homepage", "about:home");
  const { currentTab, allTabs } = useTabInfo();
  
  const openNewTab = () => {
    const gBrowser = (window as any).gBrowser;
    gBrowser.addTab("about:newtab");
  };
  
  const closeCurrentTab = () => {
    const gBrowser = (window as any).gBrowser;
    gBrowser.removeCurrentTab();
  };
  
  return (
    <VBox>
      <HBox align="center">
        <label value="ホームページ:" />
        <XULTextbox
          value={homePage()}
          onInput={setHomePage}
          flex={1}
        />
      </HBox>
      
      <HBox>
        <XULButton label="新しいタブ" onClick={openNewTab} />
        <XULButton label="タブを閉じる" onClick={closeCurrentTab} />
      </HBox>
      
      <VBox>
        <label value={`開いているタブ数: ${allTabs().length}`} />
        <label value={`現在のタブ: ${currentTab()?.label || 'なし'}`} />
      </VBox>
    </VBox>
  );
}
```

Solid-XUL により、開発者は SolidJS の強力なリアクティブシステムを活用しながら、Firefox の XUL システムとシームレスに統合できます。これにより、現代的な開発体験でブラウザの UI を構築することが可能になります。