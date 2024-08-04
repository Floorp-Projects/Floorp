use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone)]
#[serde(tag = "type")]
pub enum MixinMeta {
    #[serde(rename = "class")]
    #[serde(rename_all = "camelCase")]
    Class {
        path: String,
        class_name: String,
        extends: String,
        export: bool,
    },
    #[serde(rename = "function")]
    #[serde(rename_all = "camelCase")]
    Function {
        path: String,
        func_name: String,
        export: bool,
    },
}

impl MixinMeta {
    pub fn path(&self) -> String {
        match self {
            MixinMeta::Class {
                path,
                class_name,
                extends,
                export,
            } => path.to_string(),
            MixinMeta::Function {
                path,
                func_name,
                export,
            } => path.to_string(),
        }
    }
}

#[derive(Serialize, Deserialize, PartialEq, Clone)]
pub enum AtValue {
    #[serde(rename = "INVOKE")]
    Invoke,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct Inject {
    pub meta: InjectMeta,
    pub func: String,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct InjectMeta {
    pub method: Option<String>,
    pub at: At,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct At {
    pub value: AtValue,
    pub target: String,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct Mixin {
    pub meta: MixinMeta,
    pub inject: Vec<Inject>,
}
