import { ICU4XDataProvider, ICU4XWordSegmenter } from "icu4x";

export class SegmenterDemo {
    #displayFn: (formatted: string) => void;
    #dataProvider: ICU4XDataProvider;

    #segmenter: ICU4XWordSegmenter;
    #model: string;
    #text: string;

    constructor(displayFn: (formatted: string) => void, dataProvider: ICU4XDataProvider) {
        this.#displayFn = displayFn;
        this.#dataProvider = dataProvider;

        this.#model = "Auto";
        this.#text = "";
        this.#updateSegmenter();
    }

    setModel(model: string): void {
        this.#model = model;
        this.#updateSegmenter();
    }

    setText(text: string): void {
        this.#text = text;
        this.#render();
    }

    #updateSegmenter(): void {
        if (this.#model === "Auto") {
            this.#segmenter = ICU4XWordSegmenter.create_auto(this.#dataProvider);
        } else if (this.#model === "LSTM") {
            this.#segmenter = ICU4XWordSegmenter.create_lstm(this.#dataProvider);
        } else if (this.#model === "Dictionary") {
            this.#segmenter = ICU4XWordSegmenter.create_dictionary(this.#dataProvider);
        } else {
            console.error("Unknown model: " + this.#model);
        }
        this.#render();
    }

    #render(): void {
        const segments = [];

        let utf8Index = 0;
        let utf16Index = 0;
        const iter8 = this.#segmenter.segment_utf8(this.#text);
        while (true) {
            const next = iter8.next();

            if (next === -1) {
                segments.push(this.#text.slice(utf16Index));
                break;
            } else {
                const oldUtf16Index = utf16Index;
                while (utf8Index < next) {
                    const codePoint = this.#text.codePointAt(utf16Index);
                    const utf8Len = (codePoint <= 0x7F) ? 1
                        : (codePoint <= 0x7FF) ? 2
                        : (codePoint <= 0xFFFF) ? 3
                        : 4;
                    const utf16Len = (codePoint <= 0xFFFF) ? 1
                        : 2;
                    utf8Index += utf8Len;
                    utf16Index += utf16Len;
                }
                segments.push(this.#text.slice(oldUtf16Index, utf16Index));
            }
        }

        this.#displayFn(segments.join('<span class="seg-delim"> . </span>'));
    }
}

export function setup(dataProvider: ICU4XDataProvider): void {
    const segmentedText = document.getElementById('seg-segmented') as HTMLParagraphElement;
    const segmenterDemo = new SegmenterDemo((formatted) => {
        // Use innerHTML because we have actual HTML we want to display
        segmentedText.innerHTML = formatted;
    }, dataProvider);

    for (let btn of document.querySelectorAll<HTMLInputElement | null>('input[name="segmenter-model"]')) {
        btn?.addEventListener('click', () => segmenterDemo.setModel(btn.value));
    }

    const inputText = document.getElementById('seg-input') as HTMLTextAreaElement | null;
    inputText?.addEventListener('input', () => segmenterDemo.setText(inputText.value));

    const japaneseSamples = [
        "全部の人間は、生まれながらにして自由であり、かつ、尊厳と権利と について平等である。人間は、理性と良心とを授けられており、互いに同 胞の精神をもって行動しなければならない。",
        "全部の人は、人種、皮膚の色、性、言語、宗教、政治上その他の意見、国民的もしくは社会的出身、財産、門地その他の地位又はこれに類するいかなる自由による差別をも受けることなく、この宣言に掲げるすべての権利と自由とを享有することができる。",
        "さらに、個人の属する国又は地域が独立国であると、信託統治地域であると、非自治地域であると、又は他のなんらかの主権制限の下にあるとを問わず、その国又は地域の政治上、管轄上又は国際上の地位に基ずくいかなる差別もしてはならない。",
        "全部の人は、生命、自由と身体の安全に対する権利がある。",
        "何人も、奴隷にされ、又は苦役に服することはない。奴隷制度と奴隷売買は、いかなる形においても禁止する。",
    ];

    const sampleJapaneseBtn = document.getElementById('seg-sample-japanese') as HTMLButtonElement | null;
    sampleJapaneseBtn?.addEventListener('click', () => {
        inputText.value = japaneseSamples[Math.floor(Math.random() * japaneseSamples.length)];
        segmenterDemo.setText(inputText.value);
    });

    const chineseSamples = [
        "人人生而自由，在尊严合权利上一律平等。因赋有脾胃合道行，并着以兄弟关系的精神相对待。",
        "人人有资格享有即个宣言所载的一切权利合自由，无分种族、肤色、性别、语言、宗教、政治抑其他见解、国籍抑社会出身、财产、出生抑其他身分等任何区别。",
        "并且勿会用因一人所属的国家抑地的政治的、行政的抑国际的地位之不同而有所区别，无论即个地是独立地、托管地、非自治地抑处于其他任何主权受限制的代志之下。",
        "人人过过有权享有生命、自由合人身安全。",
        "任何人勿会用互为奴隶抑奴役；一切形式的奴隶制度合奴隶买卖，过过着予以禁止。",
    ];
    
    const sampleChineseBtn = document.getElementById('seg-sample-chinese') as HTMLButtonElement | null;
    sampleChineseBtn?.addEventListener('click', () => {
        inputText.value = chineseSamples[Math.floor(Math.random() * chineseSamples.length)];
        segmenterDemo.setText(inputText.value);
    });

    const thaiSamples = [
        "โดยที่การยอมรับนับถือเกียรติศักดิ์ประจำตัว และสิทธิเท่าเทียมกันและโอนมิได้ของบรรดา สมาชิก ทั้ง หลายแห่งครอบครัว มนุษย์เป็นหลักมูลเหตุแห่งอิสรภาพ ความยุติธรรม และสันติภาพในโลก",
        "โดยที่การไม่นำพาและการเหยียดหยามต่อสิทธิมนุษยชน ยังผลให้มีการหระทำอันป่าเถื่อน ซี่งเป็นการละเมิดมโนธรรมของมนุษยชาติอย่างร้ายแรง และใต้[ได้]มีการประกาศว่า ปณิธานสูงสุดของสามัญชนได้แก่ความต้องการให้มนุษย์มีชีวิตอยู่ในโลกด้วยอิสรภาพในการพูด และความเชื่อถือ และอิสรภาพพ้นจากความหวาดกลัวและความต้องการ",
        "โดยที่เป็นการจำเป็นอย่างยิ่งที่สิทธิมนุษยชนควรได้รับความคุ้มครองโดยหลักบังคับของกฎหมาย ถ้าไม่ประสงค์จะให้คนตกอยู่ในบังคับให้หันเข้าหาการขบถขัดขืนต่อทรราชและการกดขี่เป็นวิถีทางสุดท้าย",
    ];

    const sampleThaiBtn = document.getElementById('seg-sample-thai') as HTMLButtonElement | null;
    sampleThaiBtn?.addEventListener('click', () => {
        inputText.value = thaiSamples[Math.floor(Math.random() * thaiSamples.length)];
        segmenterDemo.setText(inputText.value);
    });
}
