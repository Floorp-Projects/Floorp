# download 警告と objdir 不整合

旧 inspection run 39003241957 の封印結果 `NO_GO_DOWNLOAD_WARNINGS` は変更しない。固定された「download warning が 0」の条件では Node の DEP0040 / DEP0169 / DEP0005 が失格になった。一方、ZIP digest、gzip digest / CRC、inventory、確認した Debug/PGO 情報をこれらの deprecation が技術的に無効化した証拠はない。

次の検証では、deprecation と integrity/config failure を別 field に残す。認識済みの runtime deprecation を記録しただけで、照合済み bytes や config を否定しない。checksum / CRC 不一致、危険な layout、出所や target の不一致、toolchain の暗黙差し替え、未実行、skip、native failure、cleanup 不明は fail のままにする。全 warning を無条件に許容する規則にはしない。

旧 objdir を今回の build に再利用しない理由は、警告とは独立して存在する。

| 旧 inspection で確認した内容 | 今回の必要条件との違い |
| --- | --- |
| debug=false、PGO=true、MOZ_PROFILE_GENERATE=true | 元 non-PGO は Debug / tests 有効。Debug 非 PGO の fresh build が必要 |
| host Linux、target x64 Darwin | 同一 job の実 native 認証には native Darwin host が必要 |
| source-repo.h が空、source / generated header / SDK / flags / linker closure の結合が不十分 | fresh source と全入力を固定し、actual config / host object / XUL / signed disposable copy を結合する必要 |
| 既存 object と XUL の存在を確認 | 修正した full host TU が再コンパイルされ、新 XUL に入り、その実 app で認証した証拠にはならない |

このため、警告条件を今後の検証で適正化しても incremental reuse の GO にはならない。旧 seal の結果と、新しい検証の受け入れ条件を混ぜない。
