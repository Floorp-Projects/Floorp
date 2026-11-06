import type {
  Article,
  Feed,
  Group,
  PrototypeState,
  Site,
  Sponsor,
  Workspace,
} from "./types";

export const groups: Group[] = [
  { id: "common", title: "共通" },
  { id: "development", title: "開発" },
  { id: "design", title: "デザイン" },
  { id: "reading", title: "読む" },
];

export const workspaces: Workspace[] = [
  { id: "development", title: "開発" },
  { id: "design", title: "デザイン" },
  { id: "personal", title: "個人" },
];

export const sponsors: Sponsor[] = [
  {
    id: "cubesoft",
    title: "Cubesoft",
    url: "https://www.cube-soft.jp/",
    icon: "cube",
  },
  {
    id: "floorp-partner",
    title: "Floorp Partner",
    url: "https://floorp.app/",
    icon: "floorp",
  },
];

const sites: Site[] = [
  {
    id: "dev-github",
    title: "GitHub",
    url: "https://github.com/",
    icon: "github",
    groupId: "development",
  },
  {
    id: "dev-floorp-docs",
    title: "Floorp Docs",
    url: "https://docs.floorp.app/",
    icon: "floorp",
    groupId: "development",
  },
  {
    id: "dev-mdn",
    title: "MDN Web Docs",
    url: "https://developer.mozilla.org/ja/",
    icon: "mdn",
    groupId: "development",
  },
  {
    id: "dev-deno",
    title: "Deno",
    url: "https://deno.com/",
    icon: "deno",
    groupId: "development",
  },
  {
    id: "dev-react",
    title: "React",
    url: "https://react.dev/",
    icon: "react",
    groupId: "development",
  },
  {
    id: "dev-typescript",
    title: "TypeScript",
    url: "https://www.typescriptlang.org/",
    icon: "typescript",
    groupId: "development",
  },
  {
    id: "dev-figma",
    title: "Figma",
    url: "https://www.figma.com/",
    icon: "figma",
    groupId: "development",
  },
  {
    id: "dev-notion",
    title: "Notion",
    url: "https://www.notion.so/",
    icon: "notion",
    groupId: "development",
  },
  {
    id: "dev-wikipedia",
    title: "Wikipedia",
    url: "https://ja.wikipedia.org/",
    icon: "wikipedia",
    groupId: "development",
  },
  {
    id: "dev-npm",
    title: "npm",
    url: "https://www.npmjs.com/",
    icon: "npm",
    groupId: "development",
  },
  {
    id: "common-github",
    title: "GitHub",
    url: "https://github.com/",
    icon: "github",
    groupId: "common",
  },
  {
    id: "common-floorp-docs",
    title: "Floorp Docs",
    url: "https://docs.floorp.app/",
    icon: "floorp",
    groupId: "common",
  },
  {
    id: "common-notion",
    title: "Notion",
    url: "https://www.notion.so/",
    icon: "notion",
    groupId: "common",
  },
  {
    id: "common-wikipedia",
    title: "Wikipedia",
    url: "https://ja.wikipedia.org/",
    icon: "wikipedia",
    groupId: "common",
  },
  {
    id: "common-mozilla",
    title: "Mozilla Blog",
    url: "https://blog.mozilla.org/",
    icon: "rss",
    groupId: "common",
  },
  {
    id: "common-mdn",
    title: "MDN Web Docs",
    url: "https://developer.mozilla.org/ja/",
    icon: "mdn",
    groupId: "common",
  },
  {
    id: "design-figma",
    title: "Figma",
    url: "https://www.figma.com/",
    icon: "figma",
    groupId: "design",
  },
  {
    id: "design-notion",
    title: "Notion",
    url: "https://www.notion.so/",
    icon: "notion",
    groupId: "design",
  },
  {
    id: "design-mdn",
    title: "MDN CSS",
    url: "https://developer.mozilla.org/ja/docs/Web/CSS",
    icon: "mdn",
    groupId: "design",
  },
  {
    id: "design-community",
    title: "Figma Community",
    url: "https://www.figma.com/community/",
    icon: "figma",
    groupId: "design",
  },
  {
    id: "reading-wikipedia",
    title: "Wikipedia",
    url: "https://ja.wikipedia.org/",
    icon: "wikipedia",
    groupId: "reading",
  },
  {
    id: "reading-mozilla",
    title: "Mozilla Blog",
    url: "https://blog.mozilla.org/",
    icon: "rss",
    groupId: "reading",
  },
  {
    id: "reading-floorp",
    title: "Floorp Blog",
    url: "https://floorp.app/",
    icon: "floorp",
    groupId: "reading",
  },
];

const feeds: Feed[] = [
  {
    id: "floorp-blog",
    title: "Floorp Blog",
    url: "https://floorp.app/",
    icon: "floorp",
  },
  {
    id: "design-notes",
    title: "デザインノート",
    url: "https://example.com/design-notes/feed.xml",
    icon: "book",
  },
  {
    id: "mozilla-blog",
    title: "Mozilla Blog",
    url: "https://blog.mozilla.org/",
    icon: "rss",
  },
];

export const articles: Article[] = [
  {
    id: "floorp-newtab",
    url: "https://floorp.app/blog/new-tab-your-way/",
    feedId: "floorp-blog",
    title: "新しいタブを、自分らしく",
    excerpt:
      "いつも開くサイトも、追いかけたい情報も。日々の作業を始める場所を、自分に合う形に整えてみませんか。",
    body: [
      "新しいタブは、次の行動を決める小さな出発点です。いつも開くサイトと、気になる記事が見やすく並んでいるだけで、日々の作業を始めやすくなります。",
      "作業に使うリンクはグループにまとめ、読みたい情報はフィードで追いかける。自分に合った入口を少しずつ整えていくことで、ブラウザーを開いた瞬間から、やりたいことに向かえます。",
    ],
    date: "2026-10-29",
    relativeTime: "30分前",
    author: "Floorp Blog",
    isExcerpt: true,
  },
  {
    id: "design-spacing",
    url: "https://example.com/design-notes/space-and-density/",
    feedId: "design-notes",
    title: "余白と密度の、ちょうどいい関係",
    excerpt:
      "情報を減らすことと、見やすくすることは別の工夫。まとまりと余白の関係から、読みやすい画面を考えます。",
    body: [
      "見やすい画面を作るために、情報をただ減らせばよいとは限りません。必要な情報がひとまとまりになり、次の操作へ自然に目が向かうことも大切です。",
      "関係する項目同士の間隔を近づけ、異なるまとまりの間には少し広い余白を置く。そんな小さな調整が、画面の構造を言葉を使わずに伝えます。",
      "一覧では複数の項目を比較できる密度を保ち、読む画面では行間や文章の幅にゆとりを持たせる。画面の役割に合わせると、同じデザインでも心地よい密度は変わります。",
      "まずは自分がよく使う画面を一つ選び、何をまとめ、どこに間隔を置くと迷いにくくなるか、ゆっくり観察してみてください。",
    ],
    date: "2026-10-29",
    relativeTime: "2時間前",
    author: "デザインノート",
    isExcerpt: false,
  },
  {
    id: "mozilla-open-web",
    url: "https://blog.mozilla.org/demo/open-web/",
    feedId: "mozilla-blog",
    title: "ひらかれたウェブを、日々の選択から",
    excerpt:
      "読む場所や使う道具を自分で選ぶこと。ウェブとの付き合い方を、毎日の小さな行動から考えてみます。",
    body: [
      "ウェブにはさまざまな場所があり、さまざまな考え方が集まっています。気になるページへリンクをたどり、自分のペースで読み進められることは、その楽しさの一つです。",
      "使う道具も、情報の受け取り方も、自分に合うものを選ぶ。読みやすい表示を整えたり、気に入ったサイトをブックマークしたりすることから、その選択は始まります。",
      "小さなサイトの記事にも、大きなメディアの情報にも、それぞれの出会いがあります。入口を一つに決めず、いくつかの窓を持つことで、日々の発見が少し広がります。",
    ],
    date: "2026-10-29",
    relativeTime: "4時間前",
    author: "Mozilla Blog",
    isExcerpt: false,
  },
  {
    id: "floorp-workspace",
    url: "https://floorp.app/blog/continue-your-work/",
    feedId: "floorp-blog",
    title: "作業の続きへ、すぐに戻る",
    excerpt:
      "開発、デザイン、個人の調べもの。作業のまとまりごとに入口を整える、小さな習慣の話です。",
    body: [
      "作業を切り替えるとき、いま必要なサイトを探し直すことがあります。よく使うリンクを作業のまとまりごとに整理すると、再開までの手間を少し減らせます。",
      "開発ではコードとドキュメントを、デザインでは制作ツールと参考資料を。目的に合うグループを選んでおくと、作業へ戻る入口が見つけやすくなります。",
      "一方で、どの作業でも読みたい記事はあります。リンクのまとまりと読む情報を、それぞれに合う形で整理すると、切り替えのたびに迷う場面が少なくなります。",
      "最初から細かく分ける必要はありません。共通のグループを出発点に、何度も探しているリンクが見えてきたら、専用のまとまりを作ってみてください。",
    ],
    date: "2026-10-28",
    relativeTime: "昨日",
    author: "Floorp Blog",
    isExcerpt: false,
  },
  {
    id: "design-reading",
    url: "https://example.com/design-notes/reading-together/",
    feedId: "design-notes",
    title: "読みたい情報を、ひとつに",
    excerpt:
      "流れていく情報の中から、自分が読みたい記事を選び取る。フィードを整理するための小さな工夫を紹介します。",
    body: [
      "新しい記事を追いかける場所が増えると、どこを確認したか分からなくなることがあります。読みたいサイトをフィードにまとめると、確認する入口を一つにできます。",
      "未読の記事を眺め、すぐ読みたいものを開き、あとで読みたいものにはスターを付ける。一度にすべてを読むより、自分の時間に合わせて選べることが大切です。",
      "記事を読む画面では、配信元と日付を分かりやすく示し、文章に集中できる幅と行間を整える。元の記事を開ける入口も、迷わず見つかる場所に置きます。",
      "読むことに使える時間は日によって違います。その日の余裕に合わせて、短い記事を一つだけ読む日があってもよいのです。",
    ],
    date: "2026-10-28",
    relativeTime: "昨日",
    author: "デザインノート",
    isExcerpt: false,
  },
  {
    id: "mozilla-rss",
    url: "https://blog.mozilla.org/demo/following-feeds/",
    feedId: "mozilla-blog",
    title: "フィードで楽しむ、ゆっくりした読書",
    excerpt:
      "更新を待ちながら、お気に入りの書き手を追いかける。フィードを通して、自分のペースで読む時間を作ります。",
    body: [
      "フィードを購読すると、いつも訪れているサイトの新着をまとめて確認できます。更新のたびにページを巡回する代わりに、届いた記事の中から読みたいものを選べます。",
      "書き手が配信した本文や抜粋を読み、気になったら元のサイトへ。ゆっくり読み返したい記事には印を付けて、自分に合う読書のリズムを作ってみてください。",
    ],
    date: "2026-10-27",
    relativeTime: "2日前",
    author: "Mozilla Blog",
    isExcerpt: true,
  },
];

export const initialState: PrototypeState = {
  settings: {
    showSearch: true,
    showDials: true,
    showFeed: true,
    showClock: true,
    layout: "balanced",
    background: {
      type: "random",
      customImage: null,
      fileName: null,
      folderPath: null,
      selectedFloorp: null,
      slideshowEnabled: false,
      slideshowInterval: 30,
    },
    translucentPanels: true,
    columns: 6,
    tileSize: "standard",
    showSponsors: true,
    refreshMinutes: 15,
  },
  assignments: {
    development: "development",
    design: "design",
    personal: "common",
  },
  sites: sites.map((site, index) => ({
    ...site,
    folderId:
      (
        {
          "dev-mdn": "dev-reference",
          "dev-wikipedia": "dev-reference",
          "dev-react": "dev-frontend",
          "dev-typescript": "dev-frontend",
          "dev-notion": "dev-projects",
          "dev-npm": "dev-projects",
        } as Record<string, string>
      )[site.id] ?? null,
    order: index + 2,
  })),
  folders: [
    {
      id: "dev-reference",
      title: "リファレンス",
      groupId: "development",
      parentId: null,
      order: 0,
    },
    {
      id: "dev-projects",
      title: "プロジェクト",
      groupId: "development",
      parentId: null,
      order: 1,
    },
    {
      id: "dev-frontend",
      title: "フロントエンド",
      groupId: "development",
      parentId: "dev-reference",
      order: 0,
    },
  ],
  feeds,
  readIds: ["design-spacing"],
  starredIds: ["design-reading"],
  dismissedSponsors: [],
};
